// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <stack>

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
class PreEvaluator final {
public:
  PreEvaluator(Prog &prog)
    : cg_(prog)
    , refs_(prog, cg_)
    , ctx_(prog)
  {
  }

  /// Start evaluation at a given function.
  bool Evaluate(Func &func);

private:
  /// Main loop, which attempts to execute the longest path in the program.
  bool Run();

  /// Check whether a function should be approximated.
  bool ShouldApproximate(Func &callee);
  /// Branch execution to another function.
  bool Call(CallSite &call);

private:
  /// Call graph of the program.
  CallGraph cg_;
  /// Set of symbols referenced by each function.
  ReferenceGraph refs_;
  /// Context, including heap and vreg mappings.
  SymbolicContext ctx_;
  /// Stack of functions being evaluated.
  std::stack<EvalContext> stk_;
};

// -----------------------------------------------------------------------------
bool PreEvaluator::Evaluate(Func &start)
{
  // Initialise the state with the start method.
  stk_.emplace(start);

  // Set up the frame.
  auto params = start.params();
  switch (unsigned n = params.size()) {
    default: llvm_unreachable("unknown argv setup");
    case 0: {
      ctx_.EnterFrame(start, {});
      break;
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
      ctx_.EnterFrame(start, { SymbolicValue::Pointer(frame, 0, 0) });
      break;
    }
  }

  // Loop until main path is exhausted.
  Run();

  // TODO: fold constants.
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
bool PreEvaluator::Run()
{
  while (!stk_.empty()) {
    // Find the node to execute.
    auto &eval = stk_.top();
    auto *node = eval.Current;
    eval.Executed.insert(node);

    // Merge in over-approximations from any other path than the main one.
    std::set<BlockEvalNode *> bypassed;
    std::set<SymbolicContext *> contexts;
    for (auto *pred : node->Preds) {
      if (pred == eval.Previous) {
        continue;
      }

      #ifndef NDEBUG
      LLVM_DEBUG(llvm::dbgs() << "=======================================\n");
      LLVM_DEBUG(llvm::dbgs()
          << "Merging ("
          << (node->Context != nullptr ? "bypassed" : "not bypassed")
          << ", "
          << (eval.Executed.count(node) ? "executed" : "not executed")
          << "):\n"
      );
      LLVM_DEBUG(llvm::dbgs() << "From: " << *pred << "\n");
      LLVM_DEBUG(llvm::dbgs() << "Into: " << *node << "\n");
      LLVM_DEBUG(llvm::dbgs() << "=======================================\n");
      #endif

      // Find all the blocks to be over-approximated.
      eval.FindBypassed(bypassed, contexts, pred);
    }

    if (!bypassed.empty()) {
      /// Approximate and merge the effects of the bypassed nodes.
      assert(!contexts.empty() && "missing context");
      SymbolicApprox(refs_, ctx_).Approximate(bypassed, contexts);
    }

    eval.Previous = node;

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

      Block *block = *node->Blocks.rbegin();
      LLVM_DEBUG(llvm::dbgs() << "=======================================\n");
      LLVM_DEBUG(llvm::dbgs() << "Evaluating " << block->getName() << "\n");
      LLVM_DEBUG(llvm::dbgs() << "=======================================\n");
      for (auto it = block->begin(); std::next(it) != block->end(); ++it) {
        SymbolicEval(refs_, ctx_).Evaluate(*it);
      }

      auto *term = block->GetTerminator();
      LLVM_DEBUG(llvm::dbgs() << *term << '\n');
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
            auto *target = eval.BlockToNode[jcc->GetTrueTarget()];
            LLVM_DEBUG(llvm::dbgs() << "\t\t-----------------------\n");
            LLVM_DEBUG(llvm::dbgs() << "\t\tJump True: " << *target << "\n");
            LLVM_DEBUG(llvm::dbgs() << "\t\t-----------------------\n");
            eval.Current = target;
            continue;
          }
          if (cond.IsFalse()) {
            // Only evaluate the false branch.
            auto *target = eval.BlockToNode[jcc->GetFalseTarget()];
            LLVM_DEBUG(llvm::dbgs() << "\t\t-----------------------\n");
            LLVM_DEBUG(llvm::dbgs() << "\t\tJump False: " << *target << "\n");
            LLVM_DEBUG(llvm::dbgs() << "\t\t-----------------------\n");
            eval.Current = target;
            continue;
          }
          break;
        }

        // Basic terminators - fall to common case which picks
        // the longest path to execute and bypasses the rest.
        case Inst::Kind::JUMP:
        case Inst::Kind::SWITCH: {
          break;
        }

        // Call sites.
        case Inst::Kind::CALL: {
          if (Call(static_cast<CallSite &>(*term))) {
            // If the call was not approximated, continue evaluating its body.
            // Otherwise, continue executing the current function, taking into
            // account the over-approximation.
            continue;
          }
          break;
        }

        // Return - following the lead of the main execution flow, find all
        // other bypassed returns, over-approximate their effects and merge
        // them into the heap before returning to the caller.
        case Inst::Kind::RETURN: {
          auto &fn = eval.GetFunc();

          LLVM_DEBUG(llvm::dbgs() << "=======================================\n");
          LLVM_DEBUG(llvm::dbgs() << "Returning " << fn.getName() << "\n");

          std::set<BlockEvalNode *> bypassed;
          std::set<SymbolicContext *> contexts;
          for (auto &node : eval.Nodes) {
            auto *ret = &*node;
            if (!node->IsReturn() || ret == eval.Current) {
              continue;
            }
            LLVM_DEBUG(llvm::dbgs() << "Joining: " << *ret << "\n");
            eval.FindBypassed(bypassed, contexts, ret);
          }

          if (!bypassed.empty()) {
            /// Approximate and merge the effects of the bypassed nodes.
            assert(!contexts.empty() && "missing context");
            SymbolicApprox(refs_, ctx_).Approximate(bypassed, contexts);
          }
          LLVM_DEBUG(llvm::dbgs() << "=======================================\n");
          for (auto &block : fn) {
            auto *term = block.GetTerminator();
            if (auto *raise = ::cast_or_null<const ReturnInst>(term)) {
              llvm_unreachable("not implemented");
            }
            if (auto *tcall = ::cast_or_null<const TailCallInst>(term)) {
              llvm_unreachable("not implemented");
            }
            assert(!term->IsReturn() && "return not handled");
          }
          continue;
        }
      }
    }

    if (node->Succs.empty()) {
      llvm_unreachable("not implemented");
    } else {
      // Queue the first successor for execution, bypass the rest.
      auto &succs = node->Succs;
      auto *succ = *succs.begin();
      eval.Current = succ;

      LLVM_DEBUG(llvm::dbgs() << "\t\tJump to node: " << *succ << "\n");
      for (auto it = std::next(succs.begin()); it != succs.end(); ++it) {
        auto *bypass = *it;
        LLVM_DEBUG(llvm::dbgs() << "\t\tBypass: " << *bypass << "\n");
        bypass->Context = std::make_unique<SymbolicContext>(ctx_);
      }
    }
  }

  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
bool PreEvaluator::ShouldApproximate(Func &callee)
{
  if (ctx_.HasFrame(callee)) {
    // Stop at recursive loops after unrolling once.
    return true;
  }
  if (callee.HasVAStart()) {
    // va_start is ABI specific, skip it.
    return true;
  }
  auto name = callee.GetName();
  if (name == "malloc" || name == "free" || name == "realloc") {
    // The memory allocator is handled separately.
    return true;
  }
  const CallGraph::Node *node = cg_[&callee];
  assert(node && "missing call graph node");
  if (node->IsRecursive()) {
    // Do not enter self-recursive functions.
    return true;
  }
  return false;
}

// -----------------------------------------------------------------------------
bool PreEvaluator::Call(CallSite &call)
{
  // Retrieve callee and arguments.
  auto callee = ctx_.Find(call.GetCallee());
  std::vector<SymbolicValue> args;
  for (auto arg : call.args()) {
    args.push_back(ctx_.Find(arg));
  }

  if (auto ptr = callee.AsPointer()) {
    // If the pointer has a unique callee, invoke it.
    if (ptr->func_size() == 1) {
      Func &func = **ptr->func_begin();
      if (ShouldApproximate(func)) {
        SymbolicApprox(refs_, ctx_).Approximate(call);
        return false;
      } else {
        LLVM_DEBUG(llvm::dbgs() << "=======================================\n");
        LLVM_DEBUG(llvm::dbgs() << "Calling: " << func.getName() << "\n");
        LLVM_DEBUG(llvm::dbgs() << "=======================================\n");

        stk_.emplace(func);
        ctx_.EnterFrame(func, args);
        return true;
      }
    } else {
      llvm_unreachable("not implemented");
    }
  } else {
    /*
    llvm_unreachable("not implemented");
    auto &call = static_cast<CallSite &>(*term);
    SymbolicApprox(refs_, ctx_).Approximate(call);
    */
    llvm_unreachable("not implemented");
  }
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
  return PreEvaluator(prog).Evaluate(*entry);
}

// -----------------------------------------------------------------------------
const char *PreEvalPass::GetPassName() const
{
  return "Partial Pre-Evaluation";
}
