// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <queue>

#include <llvm/Support/Debug.h>

#include "passes/pre_eval/eval_context.h"
#include "passes/pre_eval/reference_graph.h"
#include "passes/pre_eval/symbolic_approx.h"
#include "passes/pre_eval/symbolic_value.h"
#include "passes/pre_eval/symbolic_context.h"
#include "passes/pre_eval/symbolic_eval.h"

#define DEBUG_TYPE "pre-eval"



// -----------------------------------------------------------------------------
void SymbolicApprox::Approximate(EvalContext &eval, BlockEvalNode *node)
{
  LLVM_DEBUG(llvm::dbgs() << "=======================================\n");
  LLVM_DEBUG(llvm::dbgs() << "Over-approximating loop: " << *node << "\n");
  LLVM_DEBUG(llvm::dbgs() << "=======================================\n");

  // Start evaluating at blocks which are externally reachable.
  std::queue<Block *> q;
  for (Block *block : node->Blocks) {
    bool reached = false;
    for (Block *pred : block->predecessors()) {
      if (eval.ExecutedNodes.count(eval.BlockToNode[pred])) {
        LLVM_DEBUG(llvm::dbgs() << "Exec " << pred->getName() << " -> " << block->getName() << "\n");
        reached = true;
        break;
      }
      if (eval.Approximated.count(eval.BlockToNode[pred])) {
        LLVM_DEBUG(llvm::dbgs() << "Approx " << pred->getName() << " -> " << block->getName() << "\n");
        reached = true;
        break;
      }
    }
    if (reached) {
      LLVM_DEBUG(llvm::dbgs() << "\tEntry: " << block->getName() << "\n");
      q.push(block);
    }
  }

  eval.Approximated.insert(node);

  while (!q.empty()) {
    Block *block = q.front();
    q.pop();
    if (!node->Blocks.count(block)) {
      continue;
    }

    LLVM_DEBUG(llvm::dbgs() << "----------------------------------\n");
    LLVM_DEBUG(llvm::dbgs() << "Block: " << block->getName() << ":\n");
    LLVM_DEBUG(llvm::dbgs() << "----------------------------------\n");

    // Evaluate all instructions in the block, except the terminator.
    bool changed = false;
    for (auto it = block->begin(); std::next(it) != block->end(); ++it) {
      changed = SymbolicEval(eval, refs_, ctx_).Evaluate(*it) || changed;
    }

    // Evaluate the terminator if it is a call.
    auto *term = block->GetTerminator();
    switch (term->GetKind()) {
      default: llvm_unreachable("invalid terminator");
      case Inst::Kind::JUMP: {
        auto &jump = static_cast<JumpInst &>(*term);
        if (changed) {
          auto *target = jump.GetTarget();
          q.push(target);
        }
        break;
      }
      case Inst::Kind::JUMP_COND: {
        auto &jcc = static_cast<JumpCondInst &>(*term);
        auto *bt = jcc.GetTrueTarget();
        auto *bf = jcc.GetFalseTarget();
        if (changed) {
          if (auto *val = ctx_.FindOpt(jcc.GetCond())) {
            if (val->IsTrue()) {
              LLVM_DEBUG(llvm::dbgs() << "Fold: " << bt->getName() << "\n");
              q.push(bt);
            } else if (val->IsFalse()) {
              LLVM_DEBUG(llvm::dbgs() << "Fold: " << bf->getName() << "\n");
              q.push(bf);
            } else {
              q.push(bf);
              q.push(bt);
            }
          } else {
            q.push(bf);
            q.push(bt);
          }
        }
        break;
      }
      case Inst::Kind::TRAP: {
        break;
      }
      case Inst::Kind::CALL: {
        auto &call = static_cast<CallInst &>(*term);
        changed = Approximate(call) || changed;
        if (changed) {
          q.push(call.GetCont());
        }
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
bool SymbolicApprox::Approximate(CallSite &call)
{
  if (auto *func = call.GetDirectCallee()) {
    if (func->getName() == "malloc") {
      if (call.arg_size() == 1 && call.type_size() == 1) {
        if (auto size = ctx_.Find(call.arg(0)).AsInt()) {
          SymbolicPointer ptr = ctx_.Malloc(call, size->getZExtValue());
          LLVM_DEBUG(llvm::dbgs() << "\t\t0: " << ptr << "\n");
          return ctx_.Set(call, SymbolicValue::Value(ptr));
        } else {
          SymbolicPointer ptr = ctx_.Malloc(call, std::nullopt);
          LLVM_DEBUG(llvm::dbgs() << "\t\t0: " << ptr << "\n");
          return ctx_.Set(call, SymbolicValue::Value(ptr));
        }
      } else {
        llvm_unreachable("not implemented");
      }
    } else if (func->getName() == "free") {
      // TODO: invalidate the object?
      return false;
    } else if (func->getName() == "realloc") {
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
  std::queue<Func *> qf;
  std::set<Func *> vf;
  qf.push(&func);

  SymbolicPointer ptr;
  while (!qf.empty()) {
    auto &fn = *qf.front();
    qf.pop();
    if (!vf.insert(&fn).second) {
      continue;
    }

    auto &node = refs_.FindReferences(fn);

    std::set<Global *> globals;
    std::set<std::pair<unsigned, unsigned>> frames;
    std::set<CallSite *> sites;
    LLVM_DEBUG(llvm::dbgs() << "\t\tCall to: '" << fn.getName() << "'\n");
    for (auto *g : node.Referenced) {
      LLVM_DEBUG(llvm::dbgs() << "\t\t\t" << g->getName() << "\n");
      globals.insert(g);
    }
    for (auto arg : call.args()) {
      auto argVal = ctx_.Find(arg);
      LLVM_DEBUG(llvm::dbgs() << "\t\t\t" << argVal << "\n");
      Extract(argVal, globals, frames, sites);
    }

    ptr.LUB(ctx_.Taint(globals, frames, sites));
    LLVM_DEBUG(llvm::dbgs() << "\t\tTaint: " << ptr << "\n");
    if (node.HasRaise) {
      llvm_unreachable("not implemented");
    }

    if (node.HasIndirectCalls) {
      for (auto *func : ptr.funcs()) {
        qf.push(func);
      }
    }
  }

  auto v = SymbolicValue::Pointer(ptr);
  bool changed = ctx_.Store(ptr, v, Type::I64);
  for (unsigned i = 0, n = call.GetNumRets(); i < n; ++i) {
    changed = ctx_.Set(call.GetSubValue(i), v) || changed;
  }
  return changed;
}

// -----------------------------------------------------------------------------
void SymbolicApprox::Extract(
    const SymbolicValue &value,
    std::set<Global *> &pointers,
    std::set<std::pair<unsigned, unsigned>> &frames,
    std::set<CallSite *> &sites)
{
  switch (value.GetKind()) {
    case SymbolicValue::Kind::SCALAR:
    case SymbolicValue::Kind::UNDEFINED:
    case SymbolicValue::Kind::INTEGER:
    case SymbolicValue::Kind::LOWER_BOUNDED_INTEGER:
    case SymbolicValue::Kind::FLOAT: {
      return;
    }
    case SymbolicValue::Kind::VALUE:
    case SymbolicValue::Kind::POINTER: {
      for (auto addr : value.GetPointer()) {
        switch (addr.GetKind()) {
          case SymbolicAddress::Kind::GLOBAL: {
            pointers.insert(addr.AsGlobal().Symbol);
            return;
          }
          case SymbolicAddress::Kind::GLOBAL_RANGE: {
            pointers.insert(addr.AsGlobalRange().Symbol);
            return;
          }
          case SymbolicAddress::Kind::FRAME: {
            llvm_unreachable("not implemented");
          }
          case SymbolicAddress::Kind::FRAME_RANGE: {
            llvm_unreachable("not implemented");
          }
          case SymbolicAddress::Kind::HEAP: {
            llvm_unreachable("not implemented");
          }
          case SymbolicAddress::Kind::HEAP_RANGE: {
            sites.insert(addr.AsHeapRange().Alloc);
            return;
          }
          case SymbolicAddress::Kind::FUNC: {
            llvm_unreachable("not implemented");
          }
        }
        llvm_unreachable("invalid address kind");
      }
      return;
    }
  }
  llvm_unreachable("invalid value kind");
}

// -----------------------------------------------------------------------------
void SymbolicApprox::Approximate(
    EvalContext &eval,
    std::set<BlockEvalNode *> bypassed,
    std::set<SymbolicContext *> contexts)
{
  // Compute the union of all contexts.
  SymbolicContext &merged = **contexts.begin();
  for (auto it = std::next(contexts.begin()); it != contexts.end(); ++it) {
    merged.LUB(**it);
  }

  // If any nodes were bypassed, collect all references inside those
  // nodes, along with all additional symbols introduced in the branch.
  // Compute the transitive closure of these objects, tainting all
  // pointees with the closure as a pointer in the unified heap
  // before merging it into the current state. Map all values to this
  // tainted value, with the exception of obvious trivial constants.
  std::optional<SymbolicPointer> uses;
  std::set<CallSite *> calls;
  for (auto *node : bypassed) {
    for (Block *block : node->Blocks) {
      for (Inst &inst : *block) {
        LLVM_DEBUG(llvm::dbgs() << "\tScan " << inst << "\n");
        if (auto *call = ::cast_or_null<CallSite>(&inst)) {
          calls.insert(call);
        }
        for (Ref<Value> opValue : inst.operand_values()) {
          Ref<Inst> opInst = ::cast_or_null<Inst>(opValue);
          if (!opInst) {
            continue;
          }
          auto *usedValue = ctx_.FindOpt(*opInst);
          if (!usedValue) {
            continue;
          }
          if (auto ptr = usedValue->AsPointer()) {
            LLVM_DEBUG(llvm::dbgs() << "\t\t" << *ptr << "\n");
            if (uses) {
              uses->LUB(ptr->Decay());
            } else {
              uses.emplace(ptr->Decay());
            }
          }
        }
      }
    }
  }

  if (uses) {
    uses->LUB(ctx_.Taint(*uses));
  }
  if (!calls.empty()) {
    llvm_unreachable("not implemented");
  }

  for (auto *node : bypassed) {
    eval.Approximated.insert(node);
    for (Block *block : node->Blocks) {
      for (Inst &inst : *block) {
        LLVM_DEBUG(llvm::dbgs() << "\tApprox: " << inst << "\n");
        if (auto *mov = ::cast_or_null<MovInst>(&inst)) {
          Resolve(*mov);
        } else {
          for (unsigned i = 0, n = inst.GetNumRets(); i < n; ++i) {
            auto instRef = inst.GetSubValue(i);
            if (uses) {
              ctx_.Set(instRef, SymbolicValue::Value(*uses));
            } else {
              ctx_.Set(instRef, SymbolicValue::Scalar());
            }
          }
        }
      }
    }
  }

  // Merge the expanded prior contexts into the head.
  ctx_.LUB(merged);
}

// -----------------------------------------------------------------------------
void SymbolicApprox::Resolve(MovInst &mov)
{
  // Try to register constants introduced by mov as constants
  // instead of relying on the universal over-approximated value.
  auto arg = mov.GetArg();
  switch (arg->GetKind()) {
    case Value::Kind::INST: {
      llvm_unreachable("not implemented");
    }
    case Value::Kind::GLOBAL: {
      llvm_unreachable("not implemented");
    }
    case Value::Kind::EXPR: {
      llvm_unreachable("not implemented");
    }
    case Value::Kind::CONST: {
      auto &c = *::cast<Constant>(arg);
      switch (c.GetKind()) {
        case Constant::Kind::INT: {
          switch (auto ty = mov.GetType()) {
            case Type::I8:
            case Type::I16:
            case Type::I32:
            case Type::I64:
            case Type::V64:
            case Type::I128: {
              auto &ci = static_cast<ConstantInt &>(c);
              auto width = GetSize(ty) * 8;
              auto value = ci.GetValue();
              if (width != value.getBitWidth()) {
                ctx_.Set(mov, SymbolicValue::Integer(value.trunc(width)));
              } else {
                ctx_.Set(mov, SymbolicValue::Integer(value));
              }
              return;
            }
            case Type::F32:
            case Type::F64:
            case Type::F80:
            case Type::F128: {
              llvm_unreachable("not implemented");
            }
          }
          llvm_unreachable("invalid integer type");
        }
        case Constant::Kind::FLOAT: {
          llvm_unreachable("not implemented");
        }
      }
      llvm_unreachable("invalid constant kind");
    }
  }
  llvm_unreachable("invalid value kind");
}
