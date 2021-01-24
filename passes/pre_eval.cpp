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
#include "passes/pre_eval/symbolic_heap.h"

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
    , ctx_(heap_)
  {
  }

  /// Start evaluation at a given function.
  bool Evaluate(Func &func);

private:
  /// Main loop, which attempts to execute the longest path in the program.
  void Run();
  /// Simplify the program based on analysis results.
  bool Simplify(Func &start);
  /// Remove unreachable blocks.
  bool SimplifyCfg(SymbolicFrame &frame, SCCFunction &scc);
  /// Rewrite instructions with new values.
  bool SimplifyValues(
      std::queue<Func *> &q,
      SymbolicFrame &frame,
      SCCFunction &scc
  );
  /// Rewrite an instruction with a new value.
  Ref<Inst> Rewrite(
      SymbolicFrame &frame,
      Ref<Inst> inst,
      const SymbolicValue &v
  );

  /// Convert a value to a direct call target if possible.
  Func *FindCallee(const SymbolicValue &value);
  /// Check whether a function should be approximated.
  bool ShouldApproximate(Func &callee);
  /// Return from a function.
  template <typename T>
  void Return(T &term);
  /// Raise from an instruction.
  void Raise(RaiseInst &raise);
  /// Transfer control from one node to another.
  void Continue(SymbolicFrame *frame, Block *from, Block *to);
  /// Transfer control from one node to another.
  void Continue(SymbolicFrame *frame, SCCNode *from, Block *to);
  /// Evaluate PHIs in the successor.
  void Continue(
      const std::set<Block *> &predecessors,
      SymbolicFrame *frame,
      Block *to
  );
  /// Add additional conditions inferred from a branch.
  void Branch(SymbolicFrame *frame, Block *from, Block *to);

private:
  /// Call graph of the program.
  CallGraph cg_;
  /// Set of symbols referenced by each function.
  ReferenceGraphImpl refs_;
  /// Mapping from various objects to object IDs.
  SymbolicHeap heap_;
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
      ctx_.EnterFrame({});
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
      unsigned f = ctx_.EnterFrame({
          { 5 * 8 },
          std::nullopt,
          std::nullopt
      });
      auto &arg = ctx_.GetFrame(f, 0);
      auto p0 = std::make_shared<SymbolicPointer>(heap_.Frame(f, 0), 0);
      auto p1 = std::make_shared<SymbolicPointer>(heap_.Frame(f, 1), 0);
      auto p2 = std::make_shared<SymbolicPointer>(heap_.Frame(f, 2), 0);
      arg.Store(0, SymbolicValue::Scalar(), Type::I64);
      arg.Store(8, SymbolicValue::Scalar(), Type::I64);
      arg.Store(16, SymbolicValue::Scalar(), Type::I64);
      arg.Store(24, SymbolicValue::Pointer(p1), Type::I64);
      arg.Store(32, SymbolicValue::Pointer(p2), Type::I64);
      ctx_.EnterFrame(start, { SymbolicValue::Pointer(p0) });
      break;
    }
  }

  // Loop until main path is exhausted.
  Run();

  // Optimise the startup path based on information gathered by the analysis.
  return Simplify(start);
}

// -----------------------------------------------------------------------------
bool IsSingleUse(const Func &func)
{
  if (func.IsEntry()) {
    return false;
  }

  unsigned codeUses = 0;
  for (const User *user : func.users()) {
    if (auto *inst = ::cast_or_null<const Inst>(user)) {
      if (auto *movInst = ::cast_or_null<const MovInst>(inst)) {
        for (const User *movUsers : movInst->users()) {
          codeUses++;
        }
      } else {
        codeUses++;
      }
    } else {
      return false;
    }
  }
  return codeUses == 1;
}

// -----------------------------------------------------------------------------
Ref<Inst> PreEvaluator::Rewrite(
    SymbolicFrame &frame,
    Ref<Inst> inst,
    const SymbolicValue &v)
{
  auto add = [&] (Inst *newInst) -> Ref<Inst>
  {
    auto insert = inst->getIterator();
    while (insert->Is(Inst::Kind::PHI)) {
      ++insert;
    }
    inst->getParent()->AddInst(newInst, &*insert);
    return newInst;
  };

  auto mov = [&] (Value *newValue) -> Ref<Inst>
  {
    return add(new MovInst(inst.GetType(), newValue, inst->GetAnnots()));
  };

  auto forward = [&] () -> Ref<Inst>
  {
    if (auto orig = v.GetOrigin()) {
      if (orig->first == frame.GetIndex()) {
        auto newRef = orig->second;
        return newRef == inst ? nullptr : newRef;
      } else {
        return nullptr;
      }
    }
    return nullptr;
  };

  switch (v.GetKind()) {
    case SymbolicValue::Kind::UNDEFINED: {
      llvm_unreachable("not implemented");
    }
    case SymbolicValue::Kind::SCALAR:
    case SymbolicValue::Kind::LOWER_BOUNDED_INTEGER:
    case SymbolicValue::Kind::MASKED_INTEGER:
    case SymbolicValue::Kind::NULLABLE:
    case SymbolicValue::Kind::VALUE: {
      return forward();
    }
    case SymbolicValue::Kind::INTEGER: {
      return mov(new ConstantInt(v.GetInteger()));
    }
    case SymbolicValue::Kind::FLOAT: {
      return mov(new ConstantFloat(v.GetFloat()));
    }
    case SymbolicValue::Kind::POINTER: {
      auto ptr = v.GetPointer();
      auto pt = ptr->begin();
      if (ptr->empty() || std::next(pt) != ptr->end()) {
        return forward();
      } else {
        switch (pt->GetKind()) {
          case SymbolicAddress::Kind::OBJECT: {
            auto &o = pt->AsObject();
            auto &orig = heap_.Map(o.Object);
            switch (orig.GetKind()) {
              case SymbolicHeap::Origin::Kind::DATA: {
                auto *object = orig.AsData().Obj;
                auto *atom = &*object->begin();
                return mov(SymbolOffsetExpr::Create(atom, o.Offset));
              }
              case SymbolicHeap::Origin::Kind::FRAME: {
                // TODO: forward if in same frame.
                if (auto newInst = forward()) {
                  return newInst;
                }
                return nullptr;
              }
              case SymbolicHeap::Origin::Kind::ALLOC: {
                // TODO: forward if in same frame.
                if (auto newInst = forward()) {
                  return newInst;
                }
                return nullptr;
              }
            }
            break;
          }
          case SymbolicAddress::Kind::EXTERN: {
            return mov(pt->AsExtern().Symbol);
          }
          case SymbolicAddress::Kind::FUNC: {
            return mov(&heap_.Map(pt->AsFunc().F));
          }
          case SymbolicAddress::Kind::BLOCK: {
            return mov(pt->AsBlock().B);
          }
          case SymbolicAddress::Kind::STACK: {
            // TODO: convert to sp if in same frame.
            if (auto newInst = forward()) {
              return newInst;
            }
            return nullptr;
          }
          case SymbolicAddress::Kind::OBJECT_RANGE:
          case SymbolicAddress::Kind::EXTERN_RANGE: {
            return forward();
          }
        }
        llvm_unreachable("invalid pointer kind");
      }
    }
  }
  llvm_unreachable("invalid value kind");
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
    if (frames.size() != 1) {
      continue;
    }

    // First pass - remove branches which lead to unreachable blocks.
    auto *frame = *frames.rbegin();
    auto &scc = ctx_.GetSCCFunc(func);
    if (SimplifyCfg(*frame, scc)) {
      func.RemoveUnreachable();
      changed = true;
    }

    // Second pass - fold known values.
    if (SimplifyValues(q, *frame, scc)) {
      changed = true;
    }
  }

  return changed;
}

// -----------------------------------------------------------------------------
bool PreEvaluator::SimplifyCfg(SymbolicFrame &frame, SCCFunction &scc)
{
  bool changed = false;
  for (auto *node : scc) {
    auto *block = *node->Blocks.rbegin();
    if (node->IsLoop || !frame.IsExecuted(block)) {
      continue;
    }

    auto *term = block->GetTerminator();
    switch (term->GetKind()) {
      default: break;
      case Inst::Kind::JUMP_COND: {
        auto *jcc = static_cast<JumpCondInst *>(term);
        auto *t = jcc->GetTrueTarget();
        auto *f = jcc->GetFalseTarget();
        auto cond = frame.Find(jcc->GetCond());

        if (cond.IsTrue()) {
          // Only evaluate the true branch.
          LLVM_DEBUG(llvm::dbgs() << "Fold True: " << t->getName() << "\n");
          auto *jump = new JumpInst(t, jcc->GetAnnots());
          block->AddInst(jump, jcc);
          jcc->eraseFromParent();
          changed = true;
          break;
        }
        if (cond.IsFalse()) {
          // Only evaluate the false branch.
          LLVM_DEBUG(llvm::dbgs() << "Fold False: " << f->getName() << "\n");
          auto *jump = new JumpInst(f, jcc->GetAnnots());
          block->AddInst(jump, jcc);
          jcc->eraseFromParent();
          changed = true;
          break;
        }
        break;
      }
      case Inst::Kind::SWITCH: {
        auto *sw = static_cast<SwitchInst *>(term);
        if (auto offset = frame.Find(sw->GetIndex()).AsInt()) {
          if (offset->getBitWidth() <= 64) {
            auto idx = offset->getZExtValue();
            if (idx < sw->getNumSuccessors()) {
              auto *t = sw->getSuccessor(idx);
              LLVM_DEBUG(llvm::dbgs()
                  << "Fold " << idx << ":" << t->getName() << "\n"
              );
              auto *jump = new JumpInst(t, sw->GetAnnots());
              block->AddInst(jump, sw);
              sw->eraseFromParent();
              changed = true;
            }
          }
        }
        break;
      }
    }
  }
  return changed;
}

// -----------------------------------------------------------------------------
bool PreEvaluator::SimplifyValues(
    std::queue<Func *> &q,
    SymbolicFrame &frame,
    SCCFunction &scc)
{
  bool changed = false;
  for (auto *node : scc) {
    if (node->IsLoop) {
      // TODO
    } else {
      assert(node->Blocks.size() == 1 && "invalid block");
      Block *block = *node->Blocks.rbegin();
      if (!frame.IsExecuted(block)) {
        continue;
      }
      for (auto it = block->begin(); it != block->end(); ) {
        Inst *inst = &*it++;
        // Recurse into single-use functions.
        if (auto *call = ::cast_or_null<CallSite>(inst)) {
          if (auto *f = call->GetDirectCallee(); f && IsSingleUse(*f)) {
            q.push(f);
          }
          continue;
        }
        // Only alter instructions which do not have side effects.
        if (inst->IsVoid() || inst->IsConstant() || inst->HasSideEffects()) {
          continue;
        }

        llvm::SmallVector<Ref<Inst>, 4> newValues;
        unsigned numValues = 0;
        for (unsigned i = 0, n = inst->GetNumRets(); i < n; ++i) {
          auto ref = inst->GetSubValue(i);
          auto v = frame.Find(ref);
          Type type = ref.GetType() == Type::V64 ? Type::I64 : ref.GetType();
          const auto &annot = inst->GetAnnots();

          if (auto newInst = Rewrite(frame, inst, v)) {
            newValues.push_back(newInst);
            numValues++;
          } else {
            newValues.push_back(ref);
          }
        }

        if (numValues) {
          LLVM_DEBUG(llvm::dbgs()
            << "Replacing " << *inst << ", in " << block->getName() << "\n"
          );
          for (auto v : newValues) {
            LLVM_DEBUG(llvm::dbgs()
              << "\t" << *v << ", from " << v->getParent()->getName() << "\n"
            );
          }
          inst->replaceAllUsesWith(newValues);
          inst->eraseFromParent();
          changed = true;
        }
      }
    }
  }
  return changed;
}
// -----------------------------------------------------------------------------
Func *PreEvaluator::FindCallee(const SymbolicValue &value)
{
  auto ptr = value.AsPointer();
  if (!ptr) {
    return nullptr;
  }
  if (ptr->func_size() != 1) {
    return nullptr;
  }
  Func &func = heap_.Map(*ptr->func_begin());
  if (!ShouldApproximate(func)) {
    return &func;
  } else {
    return nullptr;
  }
}

// -----------------------------------------------------------------------------
void PreEvaluator::Run()
{
  while (auto *frame = ctx_.GetActiveFrame()) {
    // Find the node to execute.
    Block *block = frame->GetCurrentBlock();

    #ifndef NDEBUG
    LLVM_DEBUG(llvm::dbgs() << "=======================================\n");
    LLVM_DEBUG(llvm::dbgs()
        << "Evaluating " << block->getName() << " in "
        << block->getParent()->getName() << "\n"
    );
    LLVM_DEBUG(llvm::dbgs() << "=======================================\n");
    #endif

    for (auto it = block->begin(); std::next(it) != block->end(); ++it) {
      switch (it->GetKind()) {
        case Inst::Kind::PHI:
        case Inst::Kind::LANDING_PAD: {
          continue;
        }
        default: {
          SymbolicEval(heap_, *frame, refs_, ctx_, *it).Evaluate();
        }
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
        auto *t = jcc->GetTrueTarget();
        auto *f = jcc->GetFalseTarget();
        auto cond = ctx_.Find(jcc->GetCond());
        if (!frame->Limited(t) && cond.IsTrue()) {
          // Only evaluate the true branch.
          LLVM_DEBUG(llvm::dbgs() << "\t\tJump T: " << t->getName() << "\n");
          Continue(frame, block, t);
          continue;
        }
        if (!frame->Limited(f) && cond.IsFalse()) {
          // Only evaluate the false branch.
          LLVM_DEBUG(llvm::dbgs() << "\t\tJump F: " << f->getName() << "\n");
          Continue(frame, block, f);
          continue;
        }
        break;
      }

      // If possible, select the branch for a switch.
      case Inst::Kind::SWITCH: {
        auto *sw = static_cast<SwitchInst *>(term);
        if (auto offset = ctx_.Find(sw->GetIndex()).AsInt()) {
          if (offset->getBitWidth() <= 64) {
            auto idx = offset->getZExtValue();
            if (idx < sw->getNumSuccessors()) {
              auto *t = sw->getSuccessor(idx);
              if (!frame->Limited(t)) {
                LLVM_DEBUG(llvm::dbgs() << "\t\tSwitch: " << t->getName() << "\n");
                Continue(frame, block, t);
              }
              continue;
            }
          }
        }
        break;
      }

      // Basic terminators - fall to common case which picks
      // the longest path to execute and bypasses the rest.
      case Inst::Kind::JUMP: {
        auto *jmp = static_cast<JumpInst *>(term);
        auto *t = jmp->GetTarget();
        if (!frame->Limited(t)) {
          LLVM_DEBUG(llvm::dbgs() << "\t\tJump: " << t->getName() << "\n");
          Continue(frame, block, t);
          continue;
        }
        break;
      }

      // Calls which return - approximate or create frame.
      case Inst::Kind::INVOKE:
      case Inst::Kind::CALL: {
        auto &call = static_cast<CallSite &>(*term);
        // Retrieve callee an4d arguments.
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
          SymbolicApprox(refs_, heap_, ctx_).Approximate(call);
          if (call.Is(Inst::Kind::TAIL_CALL)) {
            Return(static_cast<TailCallInst &>(call));
            continue;
          }
          break;
        }
      }
      case Inst::Kind::TAIL_CALL: {
        auto &call = static_cast<TailCallInst &>(*term);
        // Retrieve callee an4d arguments.
        std::vector<SymbolicValue> args;
        for (auto arg : call.args()) {
          args.push_back(ctx_.Find(arg));
        }
        if (auto callee = FindCallee(ctx_.Find(call.GetCallee()))) {
          // Direct call - jump into the function.
          ctx_.EnterFrame(*callee, args);
        } else {
          // Unknown call - approximate and move on.
          SymbolicApprox(refs_, heap_, ctx_).Approximate(call);
          Return(call);
        }
        continue;
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

    SCCNode *node = frame->GetNode(block);
    if (node->Succs.empty()) {
      // Infinite loop with no exit - used to hang when execution finishes.
      // Do not continue execution from this point onwards.
      break;
    } else {
      Block *next = nullptr;
      while (!next) {
        // If the current node is a loop and we cannot directly jump out of
        // it, over-approximate it in its entirety.
        if (node->IsLoop) {
          LLVM_DEBUG(llvm::dbgs() << "=====================================\n");
          LLVM_DEBUG(llvm::dbgs() << "Over-approximating: " << *node << "\n");
          LLVM_DEBUG(llvm::dbgs() << "=====================================\n");

          SymbolicApprox(refs_, heap_, ctx_).Approximate(*frame, { node }, { });
          block = nullptr;
        }

        // Queue the first successor for execution, bypass the rest.
        auto &succs = node->Succs;
        auto *succ = *succs.begin();
        LLVM_DEBUG(llvm::dbgs() << "\t\tTransfer to node: " << *succ << "\n");
        for (auto it = std::next(succs.begin()); it != succs.end(); ++it) {
          LLVM_DEBUG(llvm::dbgs() << "\t\tBypass: " << **it << "\n");
          frame->Bypass(*it, ctx_);
        }

        // Approximate if the block is not unique.
        if (succ->IsLoop) {
          SymbolicApprox(refs_, heap_, ctx_).Approximate(*frame, { node }, { });
          block = nullptr;
          node = succ;
        } else {
          assert(succ->Blocks.size() == 1 && "not a loop");
          next = *succ->Blocks.begin();
        }
      }
      if (block) {
        Continue(frame, block, next);
        Branch(frame, block, next);
      } else {
        Continue(frame, node, next);
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
      if (ret->Blocks.count(calleeFrame.GetCurrentBlock()) || !ret->Exits()) {
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
      SymbolicApprox(refs_, heap_, copy).Approximate(
          calleeFrame,
          trapBypass,
          trapCtxs
      );
    }

    if (!termBypass.empty()) {
      // Approximate and merge the effects of the bypassed nodes.
      assert(!termCtxs.empty() && "missing context");
      SymbolicApprox(refs_, heap_, ctx_).Approximate(
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

    if (auto *frame = ctx_.GetActiveFrame()) {
      auto *callBlock = frame->GetCurrentBlock();
      if (!callBlock) {
        // Root frame reached - bail out.
        ctx_.LeaveRoot();
      } else {
        // If the call site produces values, map them.
        auto *callInst = ::cast<CallSite>(callBlock->GetTerminator());
        for (unsigned i = 0, n = callInst->GetNumRets(); i < n; ++i) {
          if (i < returnedValues.size()) {
            auto ref = callInst->GetSubValue(i);
            frame->Set(ref, returnedValues[i].Pin(ref, frame->GetIndex()));
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
            auto *cont = call->GetCont();
            LLVM_DEBUG(llvm::dbgs() << "\t\tReturn: " << cont->getName() << "\n");
            Continue({call->getParent()}, frame, cont);
            break;
          }
          case Inst::Kind::INVOKE: {
            llvm_unreachable("not implemented");
          }
          case Inst::Kind::TAIL_CALL: {
            continue;
          }
        }
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
    if (ret->Blocks.count(frame.GetCurrentBlock()) || !ret->Exits()) {
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
    SymbolicApprox(refs_, heap_, ctx_).Approximate(frame, bypass, ctxs);
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
    auto *retBlock = frame.GetCurrentBlock();
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
        if (!land->IsLoop) {
          assert(land->Blocks.size() == 1 && "not a loop");
          frame.Continue(*land->Blocks.begin());
        } else {
          llvm_unreachable("not implemented");
        }
        break;
      }
    }
    break;
  }
}

// -----------------------------------------------------------------------------
void PreEvaluator::Branch(SymbolicFrame *frame, Block *from, Block *to)
{
  auto *jcc = ::cast_or_null<JumpCondInst>(from->GetTerminator());
  if (!jcc) {
    return;
  }
  if (auto cmp = ::cast_or_null<CmpInst>(jcc->GetCond())) {
    bool eq = cmp->GetCC() == Cond::EQ;
    bool ne = cmp->GetCC() == Cond::NE;
    bool bt = jcc->GetTrueTarget() == to;
    bool bf = jcc->GetFalseTarget() == to;
    bool veq = (eq && bt) || (ne && bf);
    bool vne = (eq && bf) || (ne && bt);
    if (veq || vne) {
      auto vl = frame->Find(cmp->GetLHS());
      auto vr = frame->Find(cmp->GetRHS());

      if (auto i = vl.AsInt(); i && i->isNullValue() && vr.IsNullable()) {
        if (veq) {
          // TODO
          return;
        }
        if (vne) {
          frame->Set(cmp->GetRHS(), SymbolicValue::Pointer(vl.GetPointer()));
          return;
        }
      }
      if (auto i = vr.AsInt(); i && i->isNullValue() && vl.IsNullable()) {
        if (veq) {
          // TODO
          return;
        }
        if (vne) {
          auto ptr = SymbolicValue::Pointer(vl.GetPointer());
          LLVM_DEBUG(llvm::dbgs()
              << "Refining " << *cmp->GetLHS() << " to " << ptr << "\n"
          );
          frame->Set(cmp->GetLHS(), ptr);
          return;
        }
      }
      if (auto i = vl.AsInt(); veq && i) {
        // TODO
        return;
      }
      if (auto i = vr.AsInt(); veq && i) {
        // TODO
        return;
      }
    }
  }
}

// -----------------------------------------------------------------------------
void PreEvaluator::Continue(SymbolicFrame *frame, Block *from, Block *to)
{
  // Merge in over-approximations from any other path than the main one.
  // Also identify the set of predecessors who were either bypassed or
  // executed in order to determine the PHI edges that are to be executed.
  std::set<SCCNode *> bypassed;
  std::set<SymbolicContext *> ctxs;
  std::set<Block *> predecessors;
  LLVM_DEBUG(llvm::dbgs() << "=======================================\n");
  LLVM_DEBUG(llvm::dbgs() << "From: " << from->getName() << "\n");
  LLVM_DEBUG(llvm::dbgs() << "To:   " << to->getName() << "\n");
  for (auto *pred : to->predecessors()) {
    if (pred == from || frame->FindBypassed(bypassed, ctxs, pred, to)) {
      LLVM_DEBUG(llvm::dbgs() << "\t" << pred->getName() << "\n");
      predecessors.insert(pred);
    }
  }
  LLVM_DEBUG(llvm::dbgs() << "=======================================\n");

  /// Approximate and merge the effects of the bypassed nodes.
  if (!bypassed.empty()) {
    assert(!ctxs.empty() && "missing context");
    SymbolicApprox(refs_, heap_, ctx_).Approximate(*frame, bypassed, ctxs);
  }

  Continue(predecessors, frame, to);
}

// -----------------------------------------------------------------------------
void PreEvaluator::Continue(SymbolicFrame *frame, SCCNode *from, Block *to)
{
  // Merge in over-approximations from any other path than the main one.
  // Also identify the set of predecessors who were either bypassed or
  // executed in order to determine the PHI edges that are to be executed.
  std::set<SCCNode *> bypassed;
  std::set<SymbolicContext *> ctxs;
  std::set<Block *> predecessors;
  LLVM_DEBUG(llvm::dbgs() << "=======================================\n");
  LLVM_DEBUG(llvm::dbgs() << "From: " << *from << "\n");
  LLVM_DEBUG(llvm::dbgs() << "To:   " << to->getName() << "\n");
  for (auto *pred : to->predecessors()) {
    auto *predNode = frame->GetNode(pred);
    if (predNode != from && frame->FindBypassed(bypassed, ctxs, pred, to)) {
      continue;
    }
    for (Block *block : predNode->Blocks) {
      LLVM_DEBUG(llvm::dbgs() << "\t" << block->getName() << "\n");
      predecessors.insert(block);
    }
  }
  LLVM_DEBUG(llvm::dbgs() << "=======================================\n");

  /// Approximate and merge the effects of the bypassed nodes.
  if (!bypassed.empty()) {
    assert(!ctxs.empty() && "missing context");
    SymbolicApprox(refs_, heap_, ctx_).Approximate(*frame, bypassed, ctxs);
  }

  Continue(predecessors, frame, to);
}

// -----------------------------------------------------------------------------
void PreEvaluator::Continue(
    const std::set<Block *> &predecessors,
    SymbolicFrame *frame,
    Block *to)
{
  /// Evaluate PHIs in target.
  for (auto it = to->begin(); std::next(it) != to->end(); ++it) {
    auto *phi = ::cast_or_null<PhiInst>(&*it);
    if (!phi) {
      continue;
    }
    std::optional<SymbolicValue> value;
    for (unsigned i = 0, n = phi->GetNumIncoming(); i < n; ++i) {
      if (predecessors.count(phi->GetBlock(i))) {
        auto v = ctx_.Find(phi->GetValue(i));
        value = value ? value->LUB(v) : v;
      }
    }
    assert(value && "no incoming value to PHI");
    LLVM_DEBUG(llvm::dbgs() << *phi << "\n\t" << "0: " << *value << "\n");
    frame->Set(phi, *value);
  }

  /// Transfer execution to the next block.
  frame->Continue(to);
}

// -----------------------------------------------------------------------------
bool PreEvalPass::Run(Prog &prog)
{
  auto &cfg = GetConfig();
  if (!cfg.Static) {
    return false;
  }
  const std::string start = cfg.Entry.empty() ? "_start" : cfg.Entry;
  auto *entry = ::cast_or_null<Func>(prog.GetGlobal(start));
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
