// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <queue>

#include <llvm/Support/Debug.h>

#include "passes/pre_eval/reference_graph.h"
#include "passes/pre_eval/symbolic_loop.h"
#include "passes/pre_eval/symbolic_value.h"
#include "passes/pre_eval/symbolic_context.h"
#include "passes/pre_eval/symbolic_eval.h"
#include "passes/pre_eval/symbolic_approx.h"

#define DEBUG_TYPE "pre-eval"



// -----------------------------------------------------------------------------
static constexpr uint64_t kIterThreshold = 64;

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
  LLVM_DEBUG(llvm::dbgs() << "=======================================\n");
  LLVM_DEBUG(llvm::dbgs() << "Over-approximating loop: " << *node << "\n");
  LLVM_DEBUG(llvm::dbgs() << "=======================================\n");

  // Start evaluating at blocks which are externally reachable.
  std::queue<Block *> q;
  uint64_t iter = 0;
  auto queue = [&] (bool changed, Block *block, Block *from)
  {
    for (PhiInst &phi : block->phis()) {
      auto v = ctx_.Find(phi.GetValue(from));
      LLVM_DEBUG(llvm::dbgs()
          << "\tphi:" << phi << "\n\t" << from->getName() << ": " << v << "\n"
      );
      if (iter >= kIterThreshold) {
        changed = ctx_.Set(phi, Decay(v)) || changed;
      } else {
        changed = ctx_.Set(phi, v) || changed;
      }
    }
    if (changed) {
      q.push(block);
    }
  };

  for (Block *block : node->Blocks) {
    bool reached = false;
    for (Block *pred : block->predecessors()) {
      reached = reached || active.count(pred) != 0;
    }
    if (reached) {
      LLVM_DEBUG(llvm::dbgs() << "\tEntry: " << block->getName() << "\n");
      for (PhiInst &phi : block->phis()) {
        std::optional<SymbolicValue> value;
        for (unsigned i = 0, n = phi.GetNumIncoming(); i < n; ++i) {
          if (active.count(phi.GetBlock(i))) {
            auto v = ctx_.Find(phi.GetValue(i));
            value = value ? value->LUB(v) : v;
          }
        }
        assert(value && "no incoming value to PHI");
        ctx_.Set(phi, *value);
      }
      q.push(block);
    }
  }

  while (!q.empty()) {
    iter++;
    Block *block = q.front();
    q.pop();
    if (!node->Blocks.count(block)) {
      continue;
    }

    LLVM_DEBUG(llvm::dbgs() << "----------------------------------\n");
    LLVM_DEBUG(llvm::dbgs() << "Block: " << block->getName() << ", iter " << iter << ":\n");
    LLVM_DEBUG(llvm::dbgs() << "----------------------------------\n");

    // Evaluate all instructions in the block, except the terminator.
    bool changed = false;
    for (auto it = block->begin(); std::next(it) != block->end(); ++it) {
      if (it->Is(Inst::Kind::PHI)) {
        continue;
      }
      changed = SymbolicEval(frame, refs_, ctx_).Evaluate(*it) || changed;
    }

    // Evaluate the terminator if it is a call.
    auto *term = block->GetTerminator();
    switch (term->GetKind()) {
      default: llvm_unreachable("invalid terminator");
      case Inst::Kind::JUMP: {
        auto &jump = static_cast<JumpInst &>(*term);
        queue(changed, jump.GetTarget(), block);
        break;
      }
      case Inst::Kind::JUMP_COND: {
        auto &jcc = static_cast<JumpCondInst &>(*term);
        auto *bt = jcc.GetTrueTarget();
        auto *bf = jcc.GetFalseTarget();
        auto *val = ctx_.FindOpt(jcc.GetCond());
        if (val && val->IsTrue()) {
          LLVM_DEBUG(llvm::dbgs() << "Fold: " << bt->getName() << "\n");
          queue(changed, bt, block);
        } else if (val && val->IsFalse()) {
          LLVM_DEBUG(llvm::dbgs() << "Fold: " << bf->getName() << "\n");
          queue(changed, bf, block);
        } else {
          queue(changed, bf, block);
          queue(changed, bt, block);
        }
        break;
      }
      case Inst::Kind::TRAP: {
        break;
      }
      case Inst::Kind::CALL: {
        auto &call = static_cast<CallInst &>(*term);
        changed = Approximate(call) || changed;
        queue(changed, call.GetCont(), block);
        break;
      }
      case Inst::Kind::TAIL_CALL: {
        Approximate(static_cast<TailCallInst &>(*term));
        break;
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
bool SymbolicLoop::Approximate(CallSite &call)
{
  return SymbolicApprox(refs_, ctx_).Approximate(call);
}
