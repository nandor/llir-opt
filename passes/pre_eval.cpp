// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/Support/GraphWriter.h>

#include "core/block.h"
#include "core/call_graph.h"
#include "core/cast.h"
#include "core/func.h"
#include "core/insts.h"
#include "core/pass_manager.h"
#include "core/prog.h"
#include "core/analysis/dominator.h"
#include "core/analysis/loop_nesting.h"
#include "passes/pre_eval.h"
#include "passes/pre_eval/eval_context.h"
#include "passes/pre_eval/reference_graph.h"
#include "passes/pre_eval/symbolic_approx.h"
#include "passes/pre_eval/symbolic_context.h"
#include "passes/pre_eval/symbolic_eval.h"

#define DEBUG_TYPE "pre-eval"



// -----------------------------------------------------------------------------
const char *PreEvalPass::kPassID = "pre-eval";

// -----------------------------------------------------------------------------
llvm::raw_ostream &operator<<(llvm::raw_ostream &os, BlockEvalNode &node)
{
  bool first = true;
  for (Block *block :node.Blocks) {
    if (first) {
      os << ", ";
      first = false;
    }
    os << block->getName();
  }
  return os;
}

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
  auto eval = std::make_unique<EvalContext>(func);

  ctx_.EnterFrame(func, args);

  while (auto *node = eval->Current) {
    // Mark the block as executed.
    eval->Executed.insert(node);

    // Merge in over-approximations from any other path than the main one.
    std::set<BlockEvalNode *> bypassed;
    std::set<SymbolicContext *> contexts;
    for (auto *pred : node->Preds) {
      if (pred == eval->Previous) {
        continue;
      }

      #ifndef NDEBUG
      LLVM_DEBUG(llvm::dbgs() << "=======================================\n");
      LLVM_DEBUG(llvm::dbgs()
          << "Merging ("
          << (node->Context != nullptr ? "bypassed" : "not bypassed")
          << ", "
          << (eval->Executed.count(node) ? "executed" : "not executed")
          << "):\n"
      );
      LLVM_DEBUG(llvm::dbgs() << "From: " << *pred << "\n");
      LLVM_DEBUG(llvm::dbgs() << "Into: " << *node << "\n");
      LLVM_DEBUG(llvm::dbgs() << "=======================================\n");
      #endif

      // Find all the blocks to be over-approximated.
      eval->FindBypassed(bypassed, contexts, pred);
    }

    if (!bypassed.empty()) {
      /// Approximate and merge the effects of the bypassed nodes.
      assert(!contexts.empty() && "missing context");
      SymbolicApprox(refs_, ctx_).Approximate(bypassed, contexts);
    }

    eval->Previous = node;

    if (node->IsLoop) {
      // Over-approximate the effects of a loop and the functions in it.
      #ifndef NDEBUG
      LLVM_DEBUG(llvm::dbgs() << "=======================================\n");
      LLVM_DEBUG(llvm::dbgs() << "Over-approximating loop: " << *node << "\n");
      LLVM_DEBUG(llvm::dbgs() << "=======================================\n");
      #endif
      SymbolicApprox(refs_, ctx_).Approximate(node->Blocks);
    } else {
      // Evaluate all instructions in the block which is on the unique path.
      assert(node->Blocks.size() == 1 && "invalid node");

      Block *current = *node->Blocks.rbegin();
      SymbolicEval(refs_, ctx_).Evaluate(*current);

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
            eval->Current = eval->BlockToNode[jcc->GetTrueTarget()];
            LLVM_DEBUG(llvm::dbgs() << "\t\tTrue branch to: " << *node << "\n");
            continue;
          }
          if (cond.IsFalse()) {
            // Only evaluate the false branch.
            eval->Current = eval->BlockToNode[jcc->GetFalseTarget()];
            LLVM_DEBUG(llvm::dbgs() << "\t\tFalse branch to: " << *node << "\n");
          }
          break;
        }

        /// Basic terminators - fall to common case which picks
        /// the longest path to execute and bypasses the rest.
        case Inst::Kind::JUMP:
        case Inst::Kind::SWITCH:
        case Inst::Kind::CALL: {
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

      LLVM_DEBUG(llvm::dbgs() << "\t\tJump to node: " << *eval->Current << "\n");
      for (auto it = std::next(succs.begin()); it != succs.end(); ++it) {
        auto *succ = *it;
        LLVM_DEBUG(llvm::dbgs() << "\t\tBypass: " << *succ << "\n");
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
