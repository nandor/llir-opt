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
  PreEvaluator(Prog &prog) : ctx_(prog) {}

  bool Start(Func &func);
  bool Run(Func &func, llvm::ArrayRef<SymbolicValue> args);

private:
  SymbolicContext ctx_;
};


// -----------------------------------------------------------------------------
bool PreEvaluator::Start(Func &func)
{
  auto params = func.params();
  switch (unsigned n = params.size()) {
    default: llvm_unreachable("unknown argv setup");
    case 0: {
      return Run(func, {});
    }
    case 1: {
      // struct hvt_boot_info {
      //     uint64_t     mem_size;
      //     uint64_t     kernel_end;
      //     uint64_t     cpu_cycle_freq;
      //     const char * cmdline;
      //     const void * mft;
      // };
      constexpr auto numBytes = 1024;
      unsigned frame = ctx_.EnterFrame({
          Func::StackObject(0, 5 * 8, llvm::Align(8)),
          Func::StackObject(1, numBytes, llvm::Align(8)),
          Func::StackObject(2, numBytes, llvm::Align(8))
      });
      auto &arg = ctx_.GetFrame(frame, 0);
      arg.Store(24, SymbolicValue::Pointer(frame, 1, 0), Type::I64);
      arg.Store(32, SymbolicValue::Pointer(frame, 2, 0), Type::I64);
      return Run(func, { SymbolicValue::Pointer(frame, 0, 0) });
    }
  }
}

// -----------------------------------------------------------------------------
bool PreEvaluator::Run(Func &func, llvm::ArrayRef<SymbolicValue> args)
{
  auto eval = std::make_unique<FuncEvaluator>(func);

  ctx_.EnterFrame(func, args);

  while (auto *node = eval->Current) {
    if (node->IsLoop) {
      // Over-approximate the effects of a loop and the functions in it.
      #ifndef NDEBUG
      LLVM_DEBUG(llvm::dbgs() << "=======================================\n");
      LLVM_DEBUG(llvm::dbgs() << "Over-approximating loop:\n");
      for (Block *block : node->Blocks) {
        LLVM_DEBUG(llvm::dbgs() << "\t" << block->getName() << "\n");
      }
      LLVM_DEBUG(llvm::dbgs() << "=======================================\n");
      #endif
      SymbolicApprox(ctx_).Approximate(node->Blocks);
      if (node->Succs.size() == 1) {
        eval->Current = *node->Succs.begin();
      } else {
        llvm_unreachable("not implemented");
      }
    } else {
      // Evaluate all instructions in the block which is on the unique path.
      assert(node->Blocks.size() == 1 && "invalid node");
      Block *current = *node->Blocks.rbegin();

      LLVM_DEBUG(llvm::dbgs() << "=======================================\n");
      LLVM_DEBUG(llvm::dbgs() << "Evaluating " << current->getName() << "\n");
      LLVM_DEBUG(llvm::dbgs() << "=======================================\n");

      for (Inst &inst : *current) {
        SymbolicEval(ctx_).Evaluate(inst);
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
  return PreEvaluator(prog).Start(*entry);
}

// -----------------------------------------------------------------------------
const char *PreEvalPass::GetPassName() const
{
  return "Partial Pre-Evaluation";
}
