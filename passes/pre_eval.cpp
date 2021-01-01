// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/ADT/SCCIterator.h>
#include <llvm/Support/GraphWriter.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/call_graph.h"
#include "core/cfg.h"
#include "core/func.h"
#include "core/insts.h"
#include "core/pass_manager.h"
#include "core/prog.h"
#include "core/analysis/dominator.h"
#include "core/analysis/loop_nesting.h"
#include "passes/pre_eval.h"
#include "passes/pre_eval/reference_graph.h"
#include "passes/pre_eval/symbolic_approx.h"
#include "passes/pre_eval/symbolic_context.h"
#include "passes/pre_eval/symbolic_eval.h"

#define DEBUG_TYPE "pre-eval"



// -----------------------------------------------------------------------------
const char *PreEvalPass::kPassID = "pre-eval";


// -----------------------------------------------------------------------------
struct FuncEvaluator {
  struct Node {
    /// Flag indicating whether this is a loop to be over-approximated.
    bool IsLoop;
    /// Blocks which are part of the collapsed node.
    std::vector<Block *> Blocks;
    /// Set of successor nodes.
    std::vector<Node *> Succs;
    /// Set of predecessor nodes.
    std::set<Node *> Preds;
    /// Length of the longest path to an exit.
    size_t Length;
    /// Snapshot of the heap at this point.
    std::unique_ptr<SymbolicContext> Context;
  };

  /// Index of each function in reverse post-order.
  std::unordered_map<Block *, unsigned> Index;
  /// Representation of all strongly-connected components.
  std::vector<std::unique_ptr<Node>> Nodes;
  /// Mapping from blocks to SCC nodes.
  std::unordered_map<Block *, Node *> BlockToNode;
  /// Block being executed.
  Node *Current = nullptr;
  /// Previous block.
  Node *Previous = nullptr;
  /// Set of executed nodes.
  std::set<Node *> Executed;

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

      // Connect to other nodes & determine whether node is a loop.
      node->Length = it->size();
      bool isLoop = it->size() > 1;
      for (Block *block :*it) {
        for (Block *succ : block->successors()) {
          auto *succNode = BlockToNode[succ];
          if (succNode == node) {
            isLoop = true;
          } else {
            node->Succs.push_back(succNode);
            succNode->Preds.insert(node);
            node->Length = std::max(
                node->Length,
                succNode->Length + it->size()
            );
          }
        }
      }
      node->IsLoop = isLoop;

      // Sort successors by their length.
      auto &succs = node->Succs;
      std::sort(succs.begin(), succs.end(), [](Node *a, Node *b) {
        return a->Length > b->Length;
      });
      succs.erase(std::unique(succs.begin(), succs.end()), succs.end());
    }

    Current = Nodes.rbegin()->get();
  }

  bool Find(std::set<Node *> &nodes, Node *node)
  {
    if (node->Context) {
      nodes.insert(node);
      return true;
    }
    if (Executed.count(node)) {
      return false;
    }

    bool bypassed = false;
    for (Node *pred : node->Preds) {
      bypassed = Find(nodes, pred) || bypassed;
    }
    if (bypassed) {
      nodes.insert(node);
    }
    return bypassed;
  }
};

// -----------------------------------------------------------------------------
class PreEvaluator final {
public:
  PreEvaluator(Prog &prog)
    : graph_(prog)
    , refs_(prog, graph_)
    , ctx_(prog)
  {
  }

  bool Start(Func &func);
  bool Run(Func &func, llvm::ArrayRef<SymbolicValue> args);

private:
  /// Call graph of the program.
  CallGraph graph_;
  /// Set of symbols referenced by each function.
  ReferenceGraph refs_;
  /// Context, including heap and vreg mappings.
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
    // Mark the block as executed.
    eval->Executed.insert(node);

    // Merge in over-approximations from any other path than the main one.
    std::set<FuncEvaluator::Node *> bypassed;
    for (auto *pred : node->Preds) {
      if (pred == eval->Previous) {
        continue;
      }

      #ifndef NDEBUG
      LLVM_DEBUG(llvm::dbgs() << "=======================================\n");
      LLVM_DEBUG(llvm::dbgs()
          << "Merging from ("
          << (node->Context != nullptr ? "bypassed" : "not bypassed")
          << ", "
          << (eval->Executed.count(node) ? "executed" : "not executed")
          << "):\n"
      );
      for (Block *block : pred->Blocks) {
        LLVM_DEBUG(llvm::dbgs() << "\t" << block->getName() << "\n");
      }
      LLVM_DEBUG(llvm::dbgs() << "Into:\n");
      for (Block *block : node->Blocks) {
        LLVM_DEBUG(llvm::dbgs() << "\t" << block->getName() << "\n");
      }
      LLVM_DEBUG(llvm::dbgs() << "=======================================\n");
      #endif

      // Find all the blocks to be over-approximated.
      eval->Find(bypassed, pred);
    }

    if (!bypassed.empty()) {
      for (auto *node : bypassed) {
        for (Block *block : node->Blocks) {

        }
      }
      llvm_unreachable("not implemented");
    }

    eval->Previous = node;

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
      SymbolicApprox(refs_, ctx_).Approximate(node->Blocks);
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
        default: llvm_unreachable("not a terminator");

        // If possible, continue down only one branch. Otherwise, select
        // the one that leads to a longer chain and continue with it,
        // over-approximating the effects of the other.
        case Inst::Kind::JUMP_COND: {
          auto *jcc = static_cast<JumpCondInst *>(term);
          auto cond = ctx_.Find(jcc->GetCond());
          if (cond.IsTrue()) {
            // Only evaluate the true branch.
            llvm_unreachable("not implemented");
          }
          if (cond.IsFalse()) {
            // Only evaluate the false branch.
            llvm_unreachable("not implemented");
          }
          break;
        }

        /// Basic terminators - fall to common case which picks
        /// the longest path to execute and bypasses the rest.
        case Inst::Kind::JUMP:
        case Inst::Kind::SWITCH: {
          break;
        }

        /// Call - continue, over-approximating its effects, then fall through.
        case Inst::Kind::CALL: {
          auto &call = static_cast<CallInst &>(*term);
          SymbolicApprox(refs_, ctx_).Approximate(call);
          break;
        }
      }
    }

    if (node->Succs.empty()) {
      llvm_unreachable("not implemented");
    } else {
      auto &succs = node->Succs;
      // Queue the first successor for execution, bypass the rest.
      eval->Current = *succs.begin();

      #ifndef NDEBUG
      LLVM_DEBUG(llvm::dbgs() << "\t\tJump to node: \n");
      for (Block *block : eval->Current->Blocks) {
        LLVM_DEBUG(llvm::dbgs() << "\t\t\t" << block->getName() << "\n");
      }
      #endif

      for (auto it = std::next(succs.begin()); it != succs.end(); ++it) {
        LLVM_DEBUG(llvm::dbgs() << "\t\tBypass: \n");
        auto *succ = *it;
        for (Block *block : succ->Blocks) {
          LLVM_DEBUG(llvm::dbgs() << "\t\t\t" << block->getName() << "\n");
        }
        LLVM_DEBUG(llvm::dbgs() << "\n");
        succ->Context = std::make_unique<SymbolicContext>(ctx_);
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
