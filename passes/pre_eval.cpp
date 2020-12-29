// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/ADT/SCCIterator.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/cfg.h"
#include "core/func.h"
#include "core/insts.h"
#include "core/pass_manager.h"
#include "core/prog.h"
#include "core/analysis/dominator.h"
#include "core/analysis/loop_nesting.h"
#include "passes/pre_eval.h"
#include "passes/pre_eval/symbolic_approx.h"
#include "passes/pre_eval/symbolic_context.h"
#include "passes/pre_eval/symbolic_eval.h"
#include "passes/pre_eval/symbolic_heap.h"

#define DEBUG_TYPE "pre-eval"



// -----------------------------------------------------------------------------
const char *PreEvalPass::kPassID = "pre-eval";


// -----------------------------------------------------------------------------
struct FuncEvaluator {
  struct Node {
    bool IsLoop;
    std::vector<Block *> Blocks;
    std::set<Node *> Succs;
  };

  /// Index of each function in reverse post-order.
  std::unordered_map<Block *, unsigned> Index;
  /// Representation of all strongly-connected components.
  std::vector<std::unique_ptr<Node>> Nodes;
  /// Mapping from blocks to SCC nodes.
  std::unordered_map<Block *, Node *> BlockToNode;
  /// Block being executed.
  Node *Current;

  FuncEvaluator(Func &func)
  {
    for (Block *block : llvm::ReversePostOrderTraversal<Func *>(&func)) {
      Index.emplace(block, Index.size());
    }

    for (auto it = llvm::scc_begin(&func); !it.isAtEnd(); ++it) {
      Node *node = Nodes.emplace_back(std::make_unique<Node>()).get();

      for (Block *block : *it) {
        node->Blocks.push_back(block);
        BlockToNode.emplace(block, node);
      }
      std::sort(
          node->Blocks.begin(),
          node->Blocks.end(),
          [this](Block *a, Block *b) { return Index[a] < Index[b]; }
      );

      bool isLoop = it->size() > 1;
      for (Block *block :*it) {
        for (Block *succ : block->successors()) {
          auto *succNode = BlockToNode[succ];
          if (succNode == node) {
            isLoop = true;
          } else {
            node->Succs.insert(succNode);
          }
        }
      }
      node->IsLoop = isLoop;
    }

    Current = Nodes.rbegin()->get();
  }
};

// -----------------------------------------------------------------------------
class PreEvaluator final {
public:
  PreEvaluator(Prog &prog) : heap_(prog) {}

  bool Run(Func &func);

private:
  SymbolicContext ctx_;
  SymbolicHeap heap_;
};


// -----------------------------------------------------------------------------
bool PreEvaluator::Run(Func &func)
{
  auto eval = std::make_unique<FuncEvaluator>(func);

  while (auto *node = eval->Current) {
    if (node->IsLoop) {
      // Over-approximate the effects of a loop and the functions in it.
      #ifndef NDEBUG
        LLVM_DEBUG(llvm::dbgs() << "Over-approximating loop:\n");
        for (Block *block : node->Blocks) {
          LLVM_DEBUG(llvm::dbgs() << "\t" << block->getName() << "\n");
        }
      #endif
      SymbolicApprox(ctx_, heap_).Approximate(node->Blocks);
      if (node->Succs.size() == 1) {
        llvm_unreachable("not implemented");
      } else {
        llvm_unreachable("not implemented");
      }
    } else {
      // Evaluate all instructions in the block which is on the unique path.
      assert(node->Blocks.size() == 1 && "invalid node");
      Block *current = *node->Blocks.rbegin();

      LLVM_DEBUG(llvm::dbgs() << "Evaluating " << current->getName() << '\n');

      for (Inst &inst : *current) {
        LLVM_DEBUG(llvm::dbgs() << inst << '\n');
        SymbolicEval(ctx_, heap_).Dispatch(inst);
      }

      auto *term = current->GetTerminator();
      switch (term->GetKind()) {
        default: {
          term->dump();
          llvm_unreachable("not implemented");
        }
        case Inst::Kind::JUMP: {
          Block *b = static_cast<JumpInst *>(term)->GetTarget();
          LLVM_DEBUG(llvm::dbgs() << "\t\tJump to " << b->getName() << "\n\n");
          eval->Current = eval->BlockToNode[b];
          break;
        }
      }
    }
  }

  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
bool PreEvalPass::Run(Prog &prog)
{
  auto &cfg = GetConfig();
  if (!cfg.Static) {
    return false;
  }
  auto *entry = ::cast_or_null<Func>(prog.GetGlobal(cfg.Entry));
  if (!entry) {
    return false;
  }
  return PreEvaluator(prog).Run(*entry);
}

// -----------------------------------------------------------------------------
const char *PreEvalPass::GetPassName() const
{
  return "Partial Pre-Evaluation";
}
