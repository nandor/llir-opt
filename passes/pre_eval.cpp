// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <stack>

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
  void Run();

  /// Convert a value to a direct call target if possible.
  Func *FindCallee(const SymbolicValue &value);
  /// Check whether a function should be approximated.
  bool ShouldApproximate(Func &callee);

private:
  /// Call graph of the program.
  CallGraph cg_;
  /// Set of symbols referenced by each function.
  ReferenceGraph refs_;
  /// Context, including heap and vreg mappings.
  SymbolicContext ctx_;
};

// -----------------------------------------------------------------------------
bool PreEvaluator::Evaluate(Func &start)
{
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

  // Optimise the startup path based on information gathered by the analysis.
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
Func *PreEvaluator::FindCallee(const SymbolicValue &value)
{
  if (auto ptr = value.AsPointer()) {
    if (ptr->func_size() == 1) {
      Func &func = **ptr->func_begin();
      if (!ShouldApproximate(func)) {
        return &func;
      } else {
        return nullptr;
      }
    } else {
      return nullptr;
    }
  } else {
    llvm_unreachable("not implemented");
  }
}

// -----------------------------------------------------------------------------
void PreEvaluator::Run()
{
  while (auto *frame = ctx_.GetActiveFrame()) {
    // Find the node to execute.d
    auto [from, node] = frame->GetCurrentEdge();

    // Merge in over-approximations from any other path than the main one.
    // Also identify the set of predecessors who were either bypassed or
    // executed in order to determine the PHI edges that are to be executed.
    std::set<SCCNode *> bypassed;
    std::set<SymbolicContext *> contexts;
    std::set<Block *> active;
    for (auto *pred : node->Preds) {
      if (pred == from) {
        for (Block *block : pred->Blocks) {
          active.insert(block);
        }
      } else {
        #ifndef NDEBUG
        LLVM_DEBUG(llvm::dbgs() << "=======================================\n");
        LLVM_DEBUG(llvm::dbgs()
            << "Merging ("
            << (frame->IsBypassed(pred) ? "bypassed" : "not bypassed")
            << ", "
            << (frame->IsExecuted(pred) ? "executed" : "not executed")
            << "):\n"
        );
        LLVM_DEBUG(llvm::dbgs() << "From: " << *pred << "\n");
        LLVM_DEBUG(llvm::dbgs() << "Into: " << *node << "\n");
        #endif

        // Find all the blocks to be over-approximated.
        if (frame->FindBypassed(bypassed, contexts, pred, node)) {
          for (Block *block : pred->Blocks) {
            active.insert(block);
          }
        }
      }
    }

    if (!bypassed.empty()) {
      /// Approximate and merge the effects of the bypassed nodes.
      assert(!contexts.empty() && "missing context");
      SymbolicApprox(refs_, ctx_).Approximate(*frame, bypassed, contexts);
    }

    if (node->IsLoop) {
      // Over-approximate the effects of a loop and the functions in it.
      SymbolicApprox(refs_, ctx_).Approximate(*frame, active, node);
    } else {
      // Evaluate all instructions in the block which is on the unique path.
      assert(node->Blocks.size() == 1 && "invalid node");

      Block *block = *node->Blocks.rbegin();
      #ifndef NDEBUG
      LLVM_DEBUG(llvm::dbgs() << "=======================================\n");
      LLVM_DEBUG(llvm::dbgs() << "Evaluating " << block->getName() << "\n");
      for (Block *pred : active) {
        LLVM_DEBUG(llvm::dbgs() << "\t" << pred->getName() << "\n");
      }
      LLVM_DEBUG(llvm::dbgs() << "=======================================\n");
      #endif

      for (auto it = block->begin(); std::next(it) != block->end(); ++it) {
        if (auto *phi = ::cast_or_null<PhiInst>(&*it)) {
          std::optional<SymbolicValue> value;
          for (unsigned i = 0, n = phi->GetNumIncoming(); i < n; ++i) {
            if (active.count(phi->GetBlock(i))) {
              auto v = ctx_.Find(phi->GetValue(i));
              value = value ? value->LUB(v) : v;
            }
          }
          assert(value && "no incoming value to PHI");
          ctx_.Set(phi, *value);
        } else {
          SymbolicEval(*frame, refs_, ctx_).Evaluate(*it);
        }
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
            auto *target = frame->Find(jcc->GetTrueTarget());
            LLVM_DEBUG(llvm::dbgs() << "\t\tJump True: " << *target << "\n");
            frame->Continue(target);
            continue;
          }
          if (cond.IsFalse()) {
            // Only evaluate the false branch.
            auto *target = frame->Find(jcc->GetFalseTarget());
            LLVM_DEBUG(llvm::dbgs() << "\t\tJump False: " << *target << "\n");
            frame->Continue(target);
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

        // Calls which return - approximate or create frame.
        case Inst::Kind::INVOKE:
        case Inst::Kind::CALL:
        case Inst::Kind::TAIL_CALL: {
          auto &call = static_cast<CallSite &>(*term);

          // Retrieve callee and arguments.
          std::vector<SymbolicValue> args;
          for (auto arg : call.args()) {
            args.push_back(ctx_.Find(arg));
          }

          if (auto callee = FindCallee(ctx_.Find(call.GetCallee()))) {
            // Direct call - jump into the function.
            ctx_.EnterFrame(*callee, args);
            continue;
          } else {
            // Unknown call - approximate and move on.
            SymbolicApprox(refs_, ctx_).Approximate(call);
            break;
          }
        }

        // Return - following the lead of the main execution flow, find all
        // other bypassed returns, over-approximate their effects and merge
        // them into the heap before returning to the caller.
        case Inst::Kind::RETURN: {
          std::vector<SymbolicValue> returnedValues;

          // Helper method to collect all returned values.
          auto mergeReturns = [&, this] (auto *frame, auto *ret)
          {
            unsigned i = 0;
            for (auto arg : ret->args()) {
              auto v = frame->Find(arg);
              LLVM_DEBUG(llvm::dbgs() << "\t\tret <" << i << ">: " << v << "\n");
              if (i >= returnedValues.size()) {
                returnedValues.push_back(v);
              } else {
                returnedValues[i] = returnedValues[i].LUB(v);
              }
              ++i;
            }
          };
          mergeReturns(ctx_.GetActiveFrame(), static_cast<ReturnInst *>(term));

          // Traverse the chain of tail calls.
          LLVM_DEBUG(llvm::dbgs() << "=======================================\n");
          for (;;) {
            auto *calleeFrame = ctx_.GetActiveFrame();
            auto &callee = *calleeFrame->GetFunc();

            LLVM_DEBUG(llvm::dbgs() << "Returning " << callee.getName() << "\n");

            std::set<SCCNode *> bypassed;
            std::set<SymbolicContext *> contexts;
            std::set<TerminatorInst *> terms;

            for (SCCNode *ret : calleeFrame->nodes()) {
              if (!ret->Returns || ret == calleeFrame->GetCurrentNode()) {
                continue;
              }
              LLVM_DEBUG(llvm::dbgs() << "Joining: " << *ret << "\n");
              if (calleeFrame->FindBypassed(bypassed, contexts, ret, nullptr)) {
                for (Block *block : ret->Blocks) {
                  terms.insert(block->GetTerminator());
                }
              }
            }

            if (!bypassed.empty()) {
              /// Approximate and merge the effects of the bypassed nodes.
              assert(!contexts.empty() && "missing context");
              SymbolicApprox(refs_, ctx_).Approximate(*calleeFrame, bypassed, contexts);
            }

            for (auto *term : terms) {
              if (auto *ret = ::cast_or_null<ReturnInst>(term)) {
                mergeReturns(calleeFrame, ret);
              }
              if (auto *tcall = ::cast_or_null<TailCallInst>(term)) {
                mergeReturns(calleeFrame, tcall);
              }
            }

            // All done with the current frame - pop it from the stack.
            ctx_.LeaveFrame(callee);

            auto *callerFrame = ctx_.GetActiveFrame();
            auto *callNode = callerFrame->GetCurrentNode();
            assert(callNode->Blocks.size() == 1 && "invalid block");
            auto *callBlock = *callNode->Blocks.begin();

            // If the call site produces values, map them.
            auto *callInst = ::cast<CallSite>(callBlock->GetTerminator());
            for (unsigned i = 0, n = callInst->GetNumRets(); i < n; ++i) {
              if (i < returnedValues.size()) {
                callerFrame->Set(callInst->GetSubValue(i), returnedValues[i]);
              } else {
                llvm_unreachable("not implemented");
              }
            }

            // If the call is a tail call, recurse into the next frame.
            switch (callInst->GetKind()) {
              default: llvm_unreachable("invalid call instruction");
              case Inst::Kind::CALL: {
                // Returning to a call, jump to the continuation block.
                auto *call = static_cast<CallInst *>(callInst);
                auto *cont = callerFrame->Find(call->GetCont());
                LLVM_DEBUG(llvm::dbgs() << "\t\tReturn: " << *cont << "\n");
                callerFrame->Continue(cont);
                break;
              }
              case Inst::Kind::INVOKE: {
                llvm_unreachable("not implemented");
              }
              case Inst::Kind::TAIL_CALL: {
                continue;
              }
            }
            break;
          }
          LLVM_DEBUG(llvm::dbgs() << "=======================================\n");
          continue;
        }
      }
    }

    if (node->Succs.empty()) {
      // Infinite loop with no exit - used to hang when execution finishes.
      // Do not continue execution from this point onwards.
      break;
    } else {
      // Queue the first successor for execution, bypass the rest.
      auto &succs = node->Succs;
      auto *succ = *succs.begin();
      frame->Continue(succ);

      LLVM_DEBUG(llvm::dbgs() << "\t\tJump to node: " << *succ << "\n");
      for (auto it = std::next(succs.begin()); it != succs.end(); ++it) {
        LLVM_DEBUG(llvm::dbgs() << "\t\tBypass: " << **it << "\n");
        frame->Bypass(*it, ctx_);
      }
    }
  }
}

// -----------------------------------------------------------------------------
bool PreEvaluator::ShouldApproximate(Func &callee)
{
  if (callee.HasVAStart()) {
    // va_start is ABI specific, skip it.
    return true;
  }
  auto name = callee.GetName();
  if (name == "caml_alloc_small_aux" || name == "caml_alloc_shr_aux") {
    return true;
  }
  if (name == "malloc" || name == "free" || name == "realloc") {
    // The memory allocator is handled separately.
    return true;
  }
  if (name == "caml_alloc1" || name == "caml_alloc2" || name == "caml_alloc3" || name == "caml_allocN" || name == "caml_alloc_custom_mem") {
    // OCaml allocator.
    return true;
  }
  if (name == "caml_gc_dispatch" || name == "caml_check_urgent_gc" || name == "caml_alloc_small_aux" || name == "caml_alloc_shr_aux") {
    return true;
  }
  if (name == "caml_program") {
    return true;
  }
  const CallGraph::Node *node = cg_[&callee];
  assert(node && "missing call graph node");
  if (node->IsRecursive()) {
    // Do not enter self-recursive functions.
    return true;
  }
  if (ctx_.HasFrame(callee)) {
    // Stop at recursive loops after unrolling once.
    return true;
  }
  return false;
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
