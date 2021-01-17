// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <queue>

#include <llvm/Support/Debug.h>

#include "core/analysis/reference_graph.h"
#include "passes/pre_eval/symbolic_loop.h"
#include "passes/pre_eval/symbolic_value.h"
#include "passes/pre_eval/symbolic_context.h"
#include "passes/pre_eval/symbolic_eval.h"
#include "passes/pre_eval/symbolic_approx.h"

#define DEBUG_TYPE "pre-eval"



// -----------------------------------------------------------------------------
static constexpr unsigned kIterThreshold = 64;

// -----------------------------------------------------------------------------
static SymbolicValue Decay(SymbolicValue v)
{
   switch (v.GetKind()) {
    case SymbolicValue::Kind::UNDEFINED:
    case SymbolicValue::Kind::SCALAR: {
      return v;
    }
    case SymbolicValue::Kind::INTEGER:
    case SymbolicValue::Kind::FLOAT:
    case SymbolicValue::Kind::LOWER_BOUNDED_INTEGER: {
      return SymbolicValue::Scalar();
    }
    case SymbolicValue::Kind::POINTER:
    case SymbolicValue::Kind::NULLABLE:
    case SymbolicValue::Kind::VALUE: {
      return SymbolicValue::Value(v.GetPointer().Decay());
    }
  }
  llvm_unreachable("invalid value kind");
}

// -----------------------------------------------------------------------------
void SymbolicLoop::Evaluate(
    SymbolicFrame &frame,
    const std::set<Block *> &active,
    SCCNode *node)
{
  if (active.size() == 1) {
    Block *start = *active.begin();
    std::set<Block *> entries;
    for (Block *block : node->Blocks) {
      for (Block *pred : block->predecessors()) {
        if (pred == start) {
          entries.insert(block);
        }
      }
    }
    if (entries.size() == 1) {
      return Evaluate(frame, start, node, *entries.begin());
    } else {
      return Approximate(frame, active, node);
    }
  } else {
    return Approximate(frame, active, node);
  }
}

// -----------------------------------------------------------------------------
void SymbolicLoop::Evaluate(
    SymbolicFrame &frame,
    Block *from,
    SCCNode *node,
    Block *block)
{
  LLVM_DEBUG(llvm::dbgs() << "=======================================\n");
  LLVM_DEBUG(llvm::dbgs() << "Evaluating loop: " << *node << "\n");
  LLVM_DEBUG(llvm::dbgs() << "=======================================\n");

  bool changed = true;
  for (unsigned iter = 0; changed; ++iter) {
    // Bail out when jumping to a node out of the loop.
    if (!node->Blocks.count(block)) {
      return;
    }

    // Evaluate PHIs an instructions.
    changed = false;
    for (auto it = block->begin(); std::next(it) != block->end(); ++it) {
      if (auto *phi = ::cast_or_null<PhiInst>(&*it)) {
        auto v = ctx_.Find(phi->GetValue(from));
        if (iter >= kIterThreshold) {
          changed = ctx_.Set(phi, Decay(v)) || changed;
        } else {
          changed = ctx_.Set(phi, v) || changed;
        }
      } else {
        if (SymbolicEval(frame, refs_, ctx_).Evaluate(*it)) {
          LLVM_DEBUG(llvm::dbgs() << "\t\tchanged\n");
          changed = true;
        }
      }
    }

    // Update the predecessor.
    from = block;

    // Evaluate the terminator if it is a call.
    auto *term = block->GetTerminator();
    switch (term->GetKind()) {
      default: llvm_unreachable("invalid terminator");
      case Inst::Kind::JUMP: {
        auto &jump = static_cast<JumpInst &>(*term);
        block = jump.GetTarget();
        break;
      }
      case Inst::Kind::JUMP_COND: {
        auto &jcc = static_cast<JumpCondInst &>(*term);
        auto *bt = jcc.GetTrueTarget();
        auto *bf = jcc.GetFalseTarget();
        auto *val = ctx_.FindOpt(jcc.GetCond());
        if (val && val->IsTrue()) {
          LLVM_DEBUG(llvm::dbgs() << "\nContinue: " << bt->getName() << "\n\n");
          block = bt;
          continue;
        } else if (val && val->IsFalse()) {
          LLVM_DEBUG(llvm::dbgs() << "\nContinue: " << bf->getName() << "\n\n");
          block = bf;
          continue;
        } else {
          // Bail out and try to over-approximate.
          return Approximate(frame, {bt, bf}, node);
        }
        break;
      }
      case Inst::Kind::TRAP: {
        break;
      }
      case Inst::Kind::CALL: {
        auto &call = static_cast<CallInst &>(*term);
        changed = Approximate(call) || changed;
        block = call.GetCont();
        break;
      }
      case Inst::Kind::TAIL_CALL: {
        Approximate(static_cast<TailCallInst &>(*term));
        return;
      }
      case Inst::Kind::INVOKE: {
        llvm_unreachable("not implemented");
      }
      case Inst::Kind::RAISE: {
        llvm_unreachable("not implemented");
      }
    }
  }
}

// -----------------------------------------------------------------------------
void SymbolicLoop::Approximate(
    SymbolicFrame &frame,
    const std::set<Block *> &active,
    SCCNode *node)
{
  LLVM_DEBUG(llvm::dbgs() << "=======================================\n");
  LLVM_DEBUG(llvm::dbgs() << "Over-approximating loop: " << *node << "\n");
  LLVM_DEBUG(llvm::dbgs() << "=======================================\n");

  SymbolicApprox(refs_, heap_, ctx_).Approximate(frame, { node }, { });
}

// -----------------------------------------------------------------------------
bool SymbolicLoop::Approximate(CallSite &call)
{
  return SymbolicApprox(refs_, heap_, ctx_).Approximate(call);
}
