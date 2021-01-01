// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/Support/Debug.h>

#include "passes/pre_eval/reference_graph.h"
#include "passes/pre_eval/symbolic_approx.h"
#include "passes/pre_eval/symbolic_value.h"
#include "passes/pre_eval/symbolic_context.h"
#include "passes/pre_eval/symbolic_eval.h"

#define DEBUG_TYPE "pre-eval"



// -----------------------------------------------------------------------------
void SymbolicApprox::Approximate(std::vector<Block *> blocks)
{
  bool changed;
  do {
    changed = false;
    for (Block *block : blocks) {
      // Evaluate all instructions in the block, except the terminator.
      for (auto it = block->begin(); std::next(it) != block->end(); ++it) {
        if (auto *phi = ::cast_or_null<PhiInst>(&*it)) {
          std::optional<SymbolicValue> value;
          for (unsigned i = 0, n = phi->GetNumIncoming(); i < n; ++i) {
            if (auto *phiValue = ctx_.FindOpt(phi->GetValue(i))) {
              if (value) {
                value = value->LUB(*phiValue);
              } else {
                value = *phiValue;
              }
            }
          }
          assert(value && "block not yet reached");
          LLVM_DEBUG(llvm::dbgs() << *it << "\n\t0: " << *value << '\n');
          changed = ctx_.Set(*phi, *value) || changed;
        } else {
          changed = SymbolicEval(refs_, ctx_).Evaluate(*it) || changed;
        }
      }

      // Evaluate the terminator if it is a call.
      auto *term = block->GetTerminator();
      switch (term->GetKind()) {
        default: llvm_unreachable("invalid terminator");
        case Inst::Kind::JUMP:
        case Inst::Kind::JUMP_COND:
        case Inst::Kind::TRAP: {
          break;
        }
        case Inst::Kind::CALL:
        case Inst::Kind::TAIL_CALL:
        case Inst::Kind::INVOKE: {
          llvm_unreachable("not implemented");
        }
        case Inst::Kind::RAISE: {
          llvm_unreachable("not implemented");
        }
      }
    }
  } while (changed);
}

// -----------------------------------------------------------------------------
bool SymbolicApprox::Approximate(CallSite &call)
{
  if (auto *func = call.GetDirectCallee()) {
    if (func->getName() == "malloc") {
      llvm_unreachable("not implemented");
    } else if (func->getName() == "free") {
      llvm_unreachable("not implemented");
    } else {
      return Approximate(call, *func);
    }
  } else {
    llvm_unreachable("not implemented");
  }
}

// -----------------------------------------------------------------------------
bool SymbolicApprox::Approximate(CallSite &call, Func &func)
{
  auto &node = refs_.FindReferences(func);
  if (node.HasIndirectCalls || node.HasRaise) {
    llvm_unreachable("not implemented");
  } else {
    std::set<Global *> pointers;
    std::set<std::pair<unsigned, unsigned>> frames;
    LLVM_DEBUG(llvm::dbgs() << "\t\tCall to: '" << func.getName() << "'\n");
    for (auto *g : node.Referenced) {
      LLVM_DEBUG(llvm::dbgs() << "\t\t\t" << g->getName() << "\n");
    }
    for (auto arg : call.args()) {
      auto argVal = ctx_.Find(arg);
      LLVM_DEBUG(llvm::dbgs() << "\t\t\t" << argVal << "\n");
      switch (argVal.GetKind()) {
        case SymbolicValue::Kind::UNKNOWN_INTEGER:
        case SymbolicValue::Kind::UNDEFINED:
        case SymbolicValue::Kind::INTEGER: {
          continue;
        }
        case SymbolicValue::Kind::VALUE:
        case SymbolicValue::Kind::POINTER: {
          for (auto addr : argVal.GetPointer()) {
            switch (addr.GetKind()) {
              case SymbolicAddress::Kind::GLOBAL: {
                pointers.insert(addr.AsGlobal().Symbol);
                continue;
              }
              case SymbolicAddress::Kind::GLOBAL_RANGE: {
                pointers.insert(addr.AsGlobalRange().Symbol);
                continue;
              }
              case SymbolicAddress::Kind::FRAME: {
                llvm_unreachable("not implemented");
              }
              case SymbolicAddress::Kind::FRAME_RANGE: {
                llvm_unreachable("not implemented");
              }
              case SymbolicAddress::Kind::FUNC: {
                llvm_unreachable("not implemented");
              }
            }
            llvm_unreachable("invalid address kind");
          }
          continue;
        }
      }
      llvm_unreachable("invalid value kind");
    }
    auto ptr = ctx_.Taint(pointers, frames);
    auto v = SymbolicValue::Pointer(ptr);
    bool changed = ctx_.Store(ptr, v, Type::I64);
    for (unsigned i = 0, n = call.GetNumRets(); i < n; ++i) {
      changed = ctx_.Set(call.GetSubValue(i), v) || changed;
    }
    return changed;
  }
}
