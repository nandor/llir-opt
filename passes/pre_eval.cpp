// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <queue>

#include <llvm/Support/GraphWriter.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/func.h"
#include "core/insts.h"
#include "core/pass_manager.h"
#include "core/prog.h"
#include "core/analysis/call_graph.h"
#include "core/analysis/reference_graph.h"
#include "passes/pre_eval.h"
#include "passes/pre_eval/symbolic_approx.h"
#include "passes/pre_eval/symbolic_context.h"
#include "passes/pre_eval/symbolic_eval.h"
#include "passes/pre_eval/symbolic_loop.h"

#define DEBUG_TYPE "pre-eval"



// -----------------------------------------------------------------------------
const char *PreEvalPass::kPassID = "pre-eval";



// -----------------------------------------------------------------------------
namespace {
class ReferenceGraphImpl final : public ReferenceGraph {
public:
  ReferenceGraphImpl(Prog &prog, CallGraph &cg) : ReferenceGraph(prog, cg) { }

  bool Skip(Func &func) override { return false; } //return IsAllocation(func); }
};
} // namespace

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
  /// Simplify the program based on analysis results.
  bool Simplify(Func &start);

  /// Convert a value to a direct call target if possible.
  Func *FindCallee(const SymbolicValue &value);
  /// Check whether a function should be approximated.
  bool ShouldApproximate(Func &callee);
  /// Return from a function.
  template <typename T>
  void Return(T &term);
  /// Raise from an instruction.
  void Raise(RaiseInst &raise);

private:
  /// Call graph of the program.
  CallGraph cg_;
  /// Set of symbols referenced by each function.
  ReferenceGraphImpl refs_;
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
  return Simplify(start);
}

// -----------------------------------------------------------------------------
bool PreEvaluator::Simplify(Func &start)
{
  bool changed = false;
  std::queue<Func *> q;
  q.push(&start);
  while (!q.empty()) {
    Func &func = *q.front();
    q.pop();

    // Functions on the init path should have one frame.
    LLVM_DEBUG(llvm::dbgs() << "Simplifying " << func.getName() << "\n");
    auto frames = ctx_.GetFrames(func);
    assert(frames.size() == 1 && "function executed multiple times");
    auto *frame = *frames.rbegin();

    auto &scc = ctx_.GetSCCFunc(func);
    for (auto *node : scc) {
      if (!frame->IsExecuted(node)) {
        continue;
      }

      if (node->IsLoop) {
        // TODO
      } else {
        assert(node->Blocks.size() == 1 && "invalid block");
        Block *block = *node->Blocks.rbegin();
        for (auto it = block->begin(); it != block->end(); ) {
          Inst *inst = &*it++;
          // Only alter instructions which do not have side effects.
          if (inst->IsVoid() || inst->IsConstant() || inst->HasSideEffects()) {
            continue;
          }

          llvm::SmallVector<Ref<Inst>, 4> newValues;
          unsigned numValues = 0;
          for (unsigned i = 0, n = inst->GetNumRets(); i < n; ++i) {
            Inst *newInst = nullptr;
            auto ref = inst->GetSubValue(i);
            auto v = frame->Find(ref);
            Type type = ref.GetType() == Type::V64 ? Type::I64 : ref.GetType();
            const auto &annot = inst->GetAnnots();

            switch (v.GetKind()) {
              case SymbolicValue::Kind::UNDEFINED: {
                llvm_unreachable("not implemented");
              }
              case SymbolicValue::Kind::SCALAR:
              case SymbolicValue::Kind::LOWER_BOUNDED_INTEGER:
              case SymbolicValue::Kind::NULLABLE:
              case SymbolicValue::Kind::VALUE: {
                break;
              }
              case SymbolicValue::Kind::INTEGER: {
                newInst = new MovInst(type, new ConstantInt(v.GetInteger()), annot);
                break;
              }
              case SymbolicValue::Kind::FLOAT: {
                newInst = new MovInst(type, new ConstantFloat(v.GetFloat()), annot);
                break;
              }
              case SymbolicValue::Kind::POINTER: {
                auto ptr = v.GetPointer();
                auto pt = ptr.begin();
                if (!ptr.empty() && std::next(pt) == ptr.end()) {
                  switch (pt->GetKind()) {
                    case SymbolicAddress::Kind::ATOM: {
                      llvm_unreachable("not implemented");
                    }
                    case SymbolicAddress::Kind::FRAME: {
                      break;
                    }
                    case SymbolicAddress::Kind::EXTERN: {
                      auto *sym = pt->AsExtern().Symbol;
                      newInst = new MovInst(type, sym, annot);
                      break;
                    }
                    case SymbolicAddress::Kind::FUNC: {
                      auto *sym = pt->AsFunc().Fn;
                      newInst = new MovInst(type, sym, annot);
                      break;
                    }
                    case SymbolicAddress::Kind::BLOCK: {
                      llvm_unreachable("not implemented");
                    }
                    case SymbolicAddress::Kind::STACK: {
                      llvm_unreachable("not implemented");
                    }
                    case SymbolicAddress::Kind::HEAP: {
                      break;
                    }
                    case SymbolicAddress::Kind::ATOM_RANGE:
                    case SymbolicAddress::Kind::FRAME_RANGE:
                    case SymbolicAddress::Kind::HEAP_RANGE:
                    case SymbolicAddress::Kind::EXTERN_RANGE: {
                      break;
                    }
                  }
                }
                break;
              }
            }

            if (newInst) {
              auto insert = inst->getIterator();
              while (insert->Is(Inst::Kind::PHI)) {
                ++insert;
              }
              block->AddInst(newInst, &*insert);
              newValues.push_back(newInst);
              numValues++;
              changed = true;
            } else {
              newValues.push_back(ref);
            }
          }

          if (numValues) {
            inst->replaceAllUsesWith(newValues);
            inst->eraseFromParent();
          }
        }
      }
    }
    func.RemoveUnreachable();
  }

  return changed;
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
    std::set<Block *> predecessors;
    for (auto *pred : node->Preds) {
      if (pred == from) {
        for (Block *block : pred->Blocks) {
          predecessors.insert(block);
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
            predecessors.insert(block);
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
      SymbolicLoop(refs_, ctx_).Evaluate(*frame, predecessors, node);
    } else {
      // Evaluate all instructions in the block which is on the unique path.
      assert(node->Blocks.size() == 1 && "invalid node");

      Block *block = *node->Blocks.rbegin();
      #ifndef NDEBUG
      LLVM_DEBUG(llvm::dbgs() << "=======================================\n");
      LLVM_DEBUG(llvm::dbgs()
          << "Evaluating " << block->getName() << " in "
          << block->getParent()->getName() << "\n"
      );
      LLVM_DEBUG(llvm::dbgs() << "=======================================\n");
      #endif

      for (auto it = block->begin(); std::next(it) != block->end(); ++it) {
        if (auto *phi = ::cast_or_null<PhiInst>(&*it)) {
          std::optional<SymbolicValue> value;
          for (unsigned i = 0, n = phi->GetNumIncoming(); i < n; ++i) {
            if (predecessors.count(phi->GetBlock(i))) {
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
            if (call.Is(Inst::Kind::TAIL_CALL)) {
              Return(static_cast<TailCallInst &>(call));
              continue;
            }
            break;
          }
        }

        // Return - following the lead of the main execution flow, find all
        // other bypassed returns, over-approximate their effects and merge
        // them into the heap before returning to the caller.
        case Inst::Kind::RETURN: {
          Return(static_cast<ReturnInst &>(*term));
          continue;
        }
        case Inst::Kind::RAISE: {
          Raise(static_cast<RaiseInst &>(*term));
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
  if (callee.getName() == "caml_program") {
    return true;
  }
  auto name = callee.GetName();
  if (IsAllocation(callee)) {
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
template <typename T>
void PreEvaluator::Return(T &term)
{
  std::vector<SymbolicValue> returnedValues;

  // Helper method to collect all returned values.
  auto mergeReturns = [&, this] (auto &frame, auto &ret)
  {
    unsigned i = 0;
    for (auto arg : ret.args()) {
      auto v = frame.Find(arg);
      LLVM_DEBUG(llvm::dbgs() << "\t\tret <" << i << ">: " << v << "\n");
      if (i >= returnedValues.size()) {
        returnedValues.push_back(v);
      } else {
        returnedValues[i] = returnedValues[i].LUB(v);
      }
      ++i;
    }
  };
  mergeReturns(*ctx_.GetActiveFrame(), term);

  // Traverse the chain of tail calls.
  LLVM_DEBUG(llvm::dbgs() << "=======================================\n");
  for (;;) {
    auto &calleeFrame = *ctx_.GetActiveFrame();
    auto &callee = *calleeFrame.GetFunc();

    LLVM_DEBUG(llvm::dbgs() << "Returning " << callee.getName() << "\n");

    std::set<TerminatorInst *> terms;
    std::set<SCCNode *> termBypass, trapBypass;
    std::set<SymbolicContext *> termCtxs, trapCtxs;

    for (SCCNode *ret : calleeFrame.nodes()) {
      if (ret == calleeFrame.GetCurrentNode() || !ret->Exits()) {
        continue;
      }
      LLVM_DEBUG(llvm::dbgs() << "Joining: " << *ret << "\n");
      if (ret->Returns) {
        if (calleeFrame.FindBypassed(termBypass, termCtxs, ret, nullptr)) {
          for (Block *block : ret->Blocks) {
            terms.insert(block->GetTerminator());
          }
        }
      } else {
        calleeFrame.FindBypassed(trapBypass, trapCtxs, ret, nullptr);
      }
    }

    if (!trapBypass.empty()) {
      // Approximate the effect of branches which might converge to
      // a landing pad, without joining in the returning paths.
      assert(!termCtxs.empty() && "missing context");
      SymbolicContext copy(ctx_);
      SymbolicApprox(refs_, copy).Approximate(
          calleeFrame,
          trapBypass,
          trapCtxs
      );
    }

    if (!termBypass.empty()) {
      // Approximate and merge the effects of the bypassed nodes.
      assert(!termCtxs.empty() && "missing context");
      SymbolicApprox(refs_, ctx_).Approximate(
          calleeFrame,
          termBypass,
          termCtxs
      );
    }

    for (auto *term : terms) {
      if (auto *ret = ::cast_or_null<ReturnInst>(term)) {
        mergeReturns(calleeFrame, *ret);
      }
      if (auto *tcall = ::cast_or_null<TailCallInst>(term)) {
        mergeReturns(calleeFrame, *tcall);
      }
    }

    // All done with the current frame - pop it from the stack.
    ctx_.LeaveFrame(callee);

    auto *callerFrame = ctx_.GetActiveFrame();
    auto *callNode = callerFrame->GetCurrentNode();
    assert(callNode->Blocks.size() == 1 && "not a call node");
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
}

// -----------------------------------------------------------------------------
void PreEvaluator::Raise(RaiseInst &raise)
{
  // Paths which end in trap or raise are never prioritised.
  // If a function reaches a raise, it means that all executable
  // paths to it end in raises. In such a case, unify information
  // from all raising paths and find the first invoke up the chain
  // to return to with the information.
  auto &frame = *ctx_.GetActiveFrame();
  auto &callee = *frame.GetFunc();
  LLVM_DEBUG(llvm::dbgs() << "Raising " << callee.getName() << "\n");

  std::set<RaiseInst *> raises;
  std::set<SCCNode *> bypass;
  std::set<SymbolicContext *> ctxs;
  for (SCCNode *ret : frame.nodes()) {
    if (ret == frame.GetCurrentNode() || !ret->Exits()) {
      continue;
    }
    LLVM_DEBUG(llvm::dbgs() << "Joining: " << *ret << "\n");
    if (frame.FindBypassed(bypass, ctxs, ret, nullptr)) {
      for (Block *block : ret->Blocks) {
        auto *term = block->GetTerminator();
        if (auto *raise = ::cast_or_null<RaiseInst>(term)) {
          raises.insert(raise);
        }
      }
    }
  }

  if (!bypass.empty()) {
    // Approximate the effect of branches which might converge to
    // a landing pad, without joining in the returning paths.
    assert(!ctxs.empty() && "missing context");
    SymbolicApprox(refs_, ctx_).Approximate(frame, bypass, ctxs);
  }

  // Fetch the raised values and merge other raising paths.
  std::vector<SymbolicValue> raisedValues;
  for (unsigned i = 0, n = raise.arg_size(); i < n; ++i) {
    if (i < raisedValues.size()) {
      raisedValues[i] = raisedValues[i].LUB(frame.Find(raise.arg(i)));
    } else {
      raisedValues.push_back(frame.Find(raise.arg(i)));
    }
  }
  // Exit the raising frame.
  ctx_.LeaveFrame(callee);

  LLVM_DEBUG(llvm::dbgs() << "=======================================\n");
  for (auto &frame : ctx_.frames()) {
    auto *retNode = frame.GetCurrentNode();
    assert(retNode->Blocks.size() == 1 && "not a call node");
    auto *retBlock = *retNode->Blocks.begin();
    auto *term = retBlock->GetTerminator();

    switch (term->GetKind()) {
      default: llvm_unreachable("not a terminator");
      case Inst::Kind::CALL: {
        // Check whether there are any other raise or return paths.
        auto &call = static_cast<CallInst &>(*term);
        auto *contNode = frame.GetNode(call.GetCont());

        bool diverges = false;
        for (SCCNode *ret : frame.nodes()) {
          if (ret == contNode || !(ret->Returns || ret->Raises)) {
            continue;
          }
          llvm_unreachable("not implemented");
        }

        if (diverges) {
          llvm_unreachable("not implemented");
        }

        // The rest of the function is bypassed since its only
        // active control path reaches the unconditional raise
        // we are returning from.
        ctx_.LeaveFrame(*frame.GetFunc());
        continue;
      }
      case Inst::Kind::TAIL_CALL: {
        llvm_unreachable("not implemented");
      }
      case Inst::Kind::INVOKE: {
        // Continue to the landing pad of the call, bypass the
        // regular path, merging information from other return paths.
        auto &invoke = static_cast<InvokeInst &>(*term);
        frame.Bypass(frame.GetNode(invoke.GetCont()), ctx_);

        // Propagate information to landing pads.
        auto *land = frame.GetNode(invoke.GetThrow());
        for (Block *block : land->Blocks) {
          for (auto &inst : *block) {
            if (auto *land = ::cast_or_null<LandingPadInst>(&inst)) {
              LLVM_DEBUG(llvm::dbgs() << "Landing\n");
              for (unsigned i = 0, n = land->type_size(); i < n; ++i) {
                auto ref = land->GetSubValue(i);
                if (i < raisedValues.size()) {
                  const auto &val = raisedValues[i];
                  if (auto *v = ctx_.FindOpt(ref)) {
                    llvm_unreachable("not implemented");
                  } else {
                    LLVM_DEBUG(llvm::dbgs()
                        << "\t" << ref << ": " << val << "\n"
                    );
                    ctx_.Set(ref, val);
                  }
                } else {
                  llvm_unreachable("not implemented");
                }
              }
            }
          }
        }

        // Continue execution with the landing pad.
        frame.Continue(land);
        break;
      }
    }
    break;
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
