// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <set>
#include <queue>

#include <llvm/Support/Debug.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/constant.h"
#include "core/func.h"
#include "core/prog.h"
#include "core/insts.h"
#include "core/pass_manager.h"
#include "passes/sccp/lattice.h"
#include "passes/sccp/eval.h"
#include "passes/sccp/solver.h"

#define DEBUG_TYPE "sccp"



// -----------------------------------------------------------------------------
SCCPSolver::SCCPSolver(Prog &prog, const Target *target)
  : target_(target)
{
  // Identify all the arguments of all functions.
  for (Func &func : prog) {
    for (Block &block : func) {
      for (Inst &inst : block) {
        if (auto *arg = ::cast_or_null<ArgInst>(&inst)) {
          args_[&func][arg->GetIndex()].insert(arg);
        }
      }
    }
  }

  // Start exploring from externally visible functions.
  for (Func &func : prog) {
    if (!func.IsRoot() && !func.HasAddressTaken()) {
      continue;
    }
    MarkBlock(&func.getEntryBlock());
    for (auto &[idx, insts] : args_[&func]) {
      for (auto *inst : insts) {
        MarkOverdefined(*inst);
      }
    }
  }

  // Iteratively propagate values.
  while (!bottomList_.empty() || !blockList_.empty() || !instList_.empty()) {
    while (!bottomList_.empty()) {
      auto *node = bottomList_.front();
      bottomList_.pop();
      Visit(*node);
    }
    while (!instList_.empty()) {
      auto *node = instList_.front();
      instList_.pop();
      Visit(*node);
    }
    while (!blockList_.empty()) {
      auto *node = blockList_.front();
      blockList_.pop();
      Visit(node);
    }
  }
}

// -----------------------------------------------------------------------------
bool SCCPSolver::Mark(Ref<Inst> inst, const Lattice &newValue)
{
  auto &oldValue = GetValue(inst);
  if (oldValue == newValue) {
    return false;
  }
  assert(!oldValue.IsOverdefined() || newValue.IsOverdefined());
  oldValue = newValue;
  for (Use &use : inst->uses()) {
    // Ensure use is of this value.
    if (use != inst) {
      continue;
    }

    // Fetch the instruction.
    auto *inst = cast<Inst>(use.getUser());

    // If inst not yet executable, do not queue.
    if (!executable_.count(inst->getParent())) {
      continue;
    }
    // Priorities the propagation of over-defined values.
    if (newValue.IsOverdefined()) {
      bottomList_.push(inst);
    } else {
      instList_.push(inst);
    }
  }
  return true;
}

// -----------------------------------------------------------------------------
bool SCCPSolver::Mark(Ref<Inst> inst, bool f)
{
  switch (auto ty = inst.GetType()) {
    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::I64:
    case Type::I128: {
      APInt v(GetBitWidth(ty), f, true);
      return Mark(inst, Lattice::CreateInteger(v));
    }
    case Type::F32:
    case Type::F64:
    case Type::F80:
    case Type::V64:
    case Type::F128: {
      llvm_unreachable("invalid flag type");
    }
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
bool SCCPSolver::MarkEdge(Inst &inst, Block *to)
{
  Block *from = inst.getParent();

  // If the edge was marked previously, do nothing.
  if (!edges_.insert({ from, to }).second) {
    return false;
  }

  // If the block was not executable, revisit PHIs.
  if (!MarkBlock(to)) {
    for (PhiInst &phi : to->phis()) {
      VisitPhiInst(phi);
    }
  }
  return true;
}

// -----------------------------------------------------------------------------
bool SCCPSolver::MarkBlock(Block *block)
{
  if (!executable_.insert(block).second) {
    return false;
  }
  blockList_.push(block);
  return true;
}

// -----------------------------------------------------------------------------
Lattice &SCCPSolver::GetValue(Ref<Inst> inst)
{
  return values_.emplace(inst, Lattice::Unknown()).first->second;
}

// -----------------------------------------------------------------------------
void SCCPSolver::VisitArgInst(ArgInst &inst)
{
  assert(!GetValue(&inst).IsUnknown() && "missing argument value");
}

// -----------------------------------------------------------------------------
static Type LUB(Type a, Type b)
{
  if (a == b) {
    return a;
  }
  if (IsIntegerType(a) && IsIntegerType(b)) {
    return GetBitWidth(a) < GetBitWidth(b) ? b : a;
  }
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void SCCPSolver::MarkCall(CallSite &c, Func &callee, Block *cont)
{
  // Update the values of the arguments to the call.
  for (auto [i, args] : args_[&callee]) {
    auto argVal = i < c.arg_size() ? GetValue(c.arg(i)) : Lattice::Undefined();
    for (auto *arg : args) {
      auto lub = GetValue(arg).LUB(SCCPEval::Extend(argVal, arg->GetType()));
      if (lub.IsFrame()) {
        Mark(arg, Lattice::Pointer());
      } else {
        Mark(arg, lub);
      }
    }
  }

  MarkBlock(&callee.getEntryBlock());
  if (auto it = returns_.find(&callee); it != returns_.end()) {
    std::queue<std::pair<CallSite *, Block *>> q;
    std::set<const Func *> visited;
    q.emplace(&c, cont);
    while (!q.empty()) {
      auto [ci, cont] = q.front();
      q.pop();

      if (cont) {
        for (unsigned i = 0, n = ci->GetNumRets(); i < n; ++i) {
          auto ref = ci->GetSubValue(i);
          auto val = GetValue(ref);
          if (auto vt = it->second.find(i); vt != it->second.end()) {
            const auto &[pt, pv] = vt->second;
            Mark(ref, val.LUB(SCCPEval::Extend(pv, ci->type(i))));
          } else {
            Mark(ref, val);
          }
        }
        MarkEdge(*ci, cont);
      } else {
        Func *caller = ci->getParent()->getParent();
        if (!visited.insert(caller).second) {
          continue;
        }

        auto tret = returns_.emplace(caller, ResultMap{});
        auto &rets = tret.first->second;
        if (tret.second || rets != it->second) {
          for (auto [idx, val] : it->second) {
            const auto &[vt, vv] = val;
            if (idx < ci->type_size()) {
              auto ty = ci->type(idx);
              auto v = SCCPEval::Extend(vv, ty);
              auto tt = rets.emplace(idx, std::make_pair(ty, v));
              if (!tt.second) {
                auto &[pt, pv] = tt.first->second;
                pt = LUB(pt, vt);
                pv = SCCPEval::Extend(pv, pt).LUB(SCCPEval::Extend(vv, pt));
              }
            }
          }
          for (auto &[ci, cont] : calls_[caller]) {
            q.emplace(ci, cont);
          }
        }
      }
    }
  }
  calls_[&callee].emplace(&c, cont);
}

// -----------------------------------------------------------------------------
bool SCCPSolver::CanEvaluate(CallSite &inst)
{
  auto &val = GetValue(inst.GetCallee());
  if (val.IsUnknown()) {
    return false;
  }
  for (auto arg : inst.args()) {
    if (GetValue(arg).IsUnknown()) {
      return false;
    }
  }
  return true;
}

// -----------------------------------------------------------------------------
void SCCPSolver::VisitCallInst(CallInst &inst)
{
  if (!CanEvaluate(inst)) {
    return;
  }

  auto &val = GetValue(inst.GetCallee());
  if (val.IsGlobal()) {
    auto &callee = *val.GetGlobalSymbol();
    switch (callee.GetKind()) {
      case Global::Kind::FUNC: {
        // If the callee exists, connect the incoming arguments to it.
        // The callee is entered when all the arguments have known values.
        MarkCall(inst, static_cast<Func &>(callee), inst.GetCont());
        return;
      }
      case Global::Kind::EXTERN: {
        // Over-approximate everything for externs.
        MarkOverdefined(inst);
        MarkEdge(inst, inst.GetCont());
        return;
      }
      case Global::Kind::BLOCK:
      case Global::Kind::ATOM: {
        // Undefined behaviour - do not explore the continuation.
        return;
      }
    }
    llvm_unreachable("invalid global kind");
  } else {
    MarkOverdefined(inst);
    MarkEdge(inst, inst.GetCont());
  }
}

// -----------------------------------------------------------------------------
void SCCPSolver::MarkOverdefinedCall(TailCallInst &inst)
{
  std::queue<Func *> q;
  std::set<const Func *> visited;

  q.push(inst.getParent()->getParent());
  while (!q.empty()) {
    Func *f = q.front();
    q.pop();
    if (!visited.insert(f).second) {
      continue;
    }

    // Update the set of returned values of the function which returns
    // or any of the functions which reached this one through a tail call.
    auto it = returns_.emplace(f, ResultMap{});
    bool changed = it.second;
    auto &rets = it.first->second;
    for (unsigned i = 0, n = inst.type_size(); i < n; ++i) {
      if (auto it = rets.find(i); it != rets.end()) {
        auto &[vt, vv] = it->second;
        if (!vv.IsOverdefined()) {
          vv = Lattice::Overdefined();
          changed = true;
        }
      } else {
        rets.emplace(i, std::make_pair(inst.type(i), Lattice::Overdefined()));
        changed = true;
      }
    }

    // If the return values were updated, propagate information up the
    // call chain. If the callee was reached directly, mark the continuation
    // block as executable, otherwise move on to tail callers.
    if (changed) {
      for (auto &[ci, cont] : calls_[f]) {
        if (cont) {
          for (unsigned i = 0, n = ci->GetNumRets(); i < n; ++i) {
            auto ref = ci->GetSubValue(i);
            if (auto it = rets.find(i); it != rets.end()) {
              Mark(ref, Lattice::Overdefined());
            } else {
              Mark(ref, GetValue(ref));
            }
          }
          MarkEdge(*ci, cont);
        } else {
          q.push(ci->getParent()->getParent());
        }
      }
    }
  }
}

// -----------------------------------------------------------------------------
void SCCPSolver::VisitTailCallInst(TailCallInst &inst)
{
  if (!CanEvaluate(inst)) {
    return;
  }

  auto &val = GetValue(inst.GetCallee());
  auto *func = inst.getParent()->getParent();
  if (val.IsGlobal() && inst.GetCallingConv() != CallingConv::SETJMP) {
    auto &callee = *val.GetGlobalSymbol();
    switch (callee.GetKind()) {
      case Global::Kind::FUNC: {
        // Forward the call sites of the tail call to the callee.
        MarkCall(inst, static_cast<Func &>(callee), nullptr);
        return;
      }
      case Global::Kind::EXTERN: {
        // Over-approximate everything for externs.
        MarkOverdefinedCall(inst);
        return;
      }
      case Global::Kind::BLOCK:
      case Global::Kind::ATOM: {
        // Undefined behaviour - do not attempt to return.
        return;
      }
    }
    llvm_unreachable("invalid global kind");
  } else {
    MarkOverdefinedCall(inst);
  }
}

// -----------------------------------------------------------------------------
void SCCPSolver::VisitInvokeInst(InvokeInst &inst)
{
  if (!CanEvaluate(inst)) {
    return;
  }

  auto &val = GetValue(inst.GetCallee());
  if (val.IsGlobal()) {
    auto &callee = *val.GetGlobalSymbol();
    switch (callee.GetKind()) {
      case Global::Kind::FUNC: {
        // Enter the callee and also over-approximate the raise block by
        // marking is as executable. The landing pad will introduce all values
        // in an over-defined state in the target block.
        MarkCall(inst, static_cast<Func &>(callee), inst.GetCont());
        MarkEdge(inst, inst.GetThrow());
        return;
      }
      case Global::Kind::EXTERN: {
        // Over-approximate everything for externs.
        MarkOverdefined(inst);
        MarkEdge(inst, inst.GetThrow());
        MarkEdge(inst, inst.GetCont());
        return;
      }
      case Global::Kind::BLOCK:
      case Global::Kind::ATOM: {
        // Undefined behaviour - do not attempt to return.
        return;
      }
    }
    llvm_unreachable("invalid global kind");
  } else {
    // Over-approximate indirect calls.
    MarkOverdefined(inst);
    MarkEdge(inst, inst.GetCont());
    MarkEdge(inst, inst.GetThrow());
  }
}

// -----------------------------------------------------------------------------
void SCCPSolver::VisitReturnInst(ReturnInst &inst)
{
  std::set<const Func *> visited;
  std::queue<std::pair<TailCallInst *, Func *>> q;
  q.emplace(nullptr, inst.getParent()->getParent());
  while (!q.empty()) {
    auto [tcall, f] = q.front();
    q.pop();
    if (!visited.insert(f).second) {
      continue;
    }

    // Update the set of returned values of the function which returns
    // or any of the functions which reached this one through a tail call.
    auto it = returns_.emplace(f, ResultMap{});
    auto &rets = it.first->second;
    if (it.second) {
      // First time returning - insert the values.
      for (unsigned i = 0, n = inst.arg_size(); i < n; ++i) {
        auto arg = inst.arg(i);
        auto ty = arg.GetType();
        auto v = GetValue(arg);
        if (!tcall || i < tcall->type_size()) {
          rets.emplace(i, std::make_pair(ty, GetValue(arg)));
        }
      }
    } else {
      // Previous returns occurred - consider missing values to be undef.
      // Add the LUB of the newly returned value and the old one or undef.
      for (unsigned i = 0, n = inst.arg_size(); i < n; ++i) {
        auto arg = inst.arg(i);
        auto ty = arg.GetType();
        auto v = GetValue(arg);
        if (!tcall || i < tcall->type_size()) {
          if (auto it = rets.find(i); it != rets.end()) {
            auto &[pt, pv] = it->second;
            pt = LUB(pt, ty);
            pv = SCCPEval::Extend(pv, pt).LUB(SCCPEval::Extend(v, pt));
          } else if (!v.IsUndefined()) {
            rets.emplace(i, std::make_pair(ty, v));
          }
        }
      }
    }

    // If the return values were updated, propagate information up the
    // call chain. If the callee was reached directly, mark the continuation
    // block as executable, otherwise move on to tail callers.
    for (auto &[ci, cont] : calls_[f]) {
      if (cont) {
        for (unsigned i = 0, n = ci->GetNumRets(); i < n; ++i) {
          auto ref = ci->GetSubValue(i);
          auto val = GetValue(ref);
          if (auto it = rets.find(i); it != rets.end()) {
            const auto &[vt, vv] = it->second;
            Mark(ref, val.LUB(SCCPEval::Extend(vv, ci->type(i))));
          } else {
            Mark(ref, val);
          }
        }
        MarkEdge(*ci, cont);
      } else {
        q.emplace(::cast<TailCallInst>(ci), ci->getParent()->getParent());
      }
    }
  }
}

// -----------------------------------------------------------------------------
void SCCPSolver::VisitUnaryInst(UnaryInst &inst)
{
  auto &argVal = GetValue(inst.GetArg());
  if (argVal.IsUnknown()) {
    return;
  }

  Mark(inst, SCCPEval::Eval(&inst, argVal));
}

// -----------------------------------------------------------------------------
void SCCPSolver::VisitBinaryInst(BinaryInst &inst)
{
  auto &lhsVal = GetValue(inst.GetLHS());
  auto &rhsVal = GetValue(inst.GetRHS());
  if (lhsVal.IsUnknown() || rhsVal.IsUnknown()) {
    return;
  }

  Mark(inst, SCCPEval::Eval(&inst, lhsVal, rhsVal));
}

// -----------------------------------------------------------------------------
void SCCPSolver::VisitJumpInst(JumpInst &inst)
{
  MarkEdge(inst, inst.GetTarget());
}

// -----------------------------------------------------------------------------
void SCCPSolver::VisitJumpCondInst(JumpCondInst &inst)
{
  auto &val = GetValue(inst.GetCond());
  if (val.IsUnknown()) {
    return;
  }

  if (!val.IsUndefined()) {
    if (!val.IsTrue()) {
      MarkEdge(inst, inst.GetFalseTarget());
    }
    if (!val.IsFalse()) {
      MarkEdge(inst, inst.GetTrueTarget());
    }
  } else {
    MarkEdge(inst, inst.GetFalseTarget());
  }
}

// -----------------------------------------------------------------------------
void SCCPSolver::VisitSwitchInst(SwitchInst &inst)
{
  auto &val = GetValue(inst.GetIndex());
  switch (val.GetKind()) {
    case Lattice::Kind::UNKNOWN: {
      return;
    }
    case Lattice::Kind::FRAME:
    case Lattice::Kind::GLOBAL:
    case Lattice::Kind::POINTER:
    case Lattice::Kind::RANGE:
    case Lattice::Kind::OVERDEFINED:
    case Lattice::Kind::MASK:
    case Lattice::Kind::FLOAT:
    case Lattice::Kind::FLOAT_ZERO: {
      for (unsigned i = 0; i < inst.getNumSuccessors(); ++i) {
        MarkEdge(inst, inst.getSuccessor(i));
      }
      return;
    }
    case Lattice::Kind::INT: {
      auto index = val.GetInt().getSExtValue();
      if (index < inst.getNumSuccessors()) {
        MarkEdge(inst, inst.getSuccessor(index));
      }
      return;
    }
    case Lattice::Kind::UNDEFINED: {
      MarkEdge(inst, inst.getSuccessor(0));
      return;
    }
  }
  llvm_unreachable("invalid value kind");
}

// -----------------------------------------------------------------------------
void SCCPSolver::VisitSelectInst(SelectInst &inst)
{
  auto &cond = GetValue(inst.GetCond());
  auto &valTrue = GetValue(inst.GetTrue());
  auto &valFalse = GetValue(inst.GetFalse());
  if (cond.IsUnknown() || valTrue.IsUnknown() || valFalse.IsUnknown()) {
    return;
  }

  if (cond.IsTrue()) {
    Mark(inst, valTrue);
  } else if (cond.IsFalse()) {
    Mark(inst, valFalse);
  } else if (cond.IsUndefined()) {
    Mark(inst, Lattice::Undefined());
  } else {
    Mark(inst, valTrue.LUB(valFalse));
  }
}

// -----------------------------------------------------------------------------
void SCCPSolver::VisitFrameInst(FrameInst &inst)
{
  Mark(inst, Lattice::CreateFrame(inst.GetObject(), inst.GetOffset()));
}

// -----------------------------------------------------------------------------
static Lattice FloatFromInt(const APInt &val, const llvm::fltSemantics &sema)
{
  switch (val.getBitWidth()) {
    default: {
      llvm_unreachable("invalid float size");
    }
    case 32: {
      llvm_unreachable("not implemented");
    }
    case 64: {
      APFloat f(APFloat::IEEEdouble(), val);
      bool lossy = false;
      f.convert(sema, llvm::APFloat::rmNearestTiesToEven, &lossy);
      return lossy ? Lattice::Overdefined() : Lattice::CreateFloat(f);
    }
  }
}

// -----------------------------------------------------------------------------
void SCCPSolver::VisitMovInst(MovInst &inst)
{
  auto ty = inst.GetType();
  auto value = inst.GetArg();
  switch (value->GetKind()) {
    case Value::Kind::INST: {
      Mark(inst, GetValue(cast<Inst>(value)));
      return;
    }
    case Value::Kind::GLOBAL: {
      Mark(inst, Lattice::CreateGlobal(&*cast<Global>(value)));
      return;
    }
    case Value::Kind::EXPR: {
      Expr &e = *cast<Expr>(value);
      switch (e.GetKind()) {
        case Expr::Kind::SYMBOL_OFFSET: {
          auto &sym = static_cast<SymbolOffsetExpr &>(e);
          Mark(inst, Lattice::CreateGlobal(sym.GetSymbol(), sym.GetOffset()));
          return;
        }
      }
      llvm_unreachable("invalid expression");
    }
    case Value::Kind::CONST: {
      Constant &c = *cast<Constant>(value);
      union U { int64_t i; double d; };
      switch (c.GetKind()) {
        case Constant::Kind::INT: {
          const auto &i = static_cast<ConstantInt &>(c).GetValue();
          switch (ty) {
            case Type::I8:
            case Type::I16:
            case Type::I32:
            case Type::I64:
            case Type::V64:
            case Type::I128: {
              Mark(inst, SCCPEval::Extend(Lattice::CreateInteger(i), ty));
              return;
            }
            case Type::F32: {
              Mark(inst, FloatFromInt(i, APFloat::IEEEsingle()));
              return;
            }
            case Type::F64: {
              Mark(inst, FloatFromInt(i, APFloat::IEEEdouble()));
              return;
            }
            case Type::F80:
            case Type::F128: {
              Mark(inst, Lattice::Overdefined());
              return;
            }
          }
          llvm_unreachable("invalid type");
          break;
        }
        case Constant::Kind::FLOAT: {
          const auto &f = static_cast<ConstantFloat &>(c).GetValue();
          switch (ty) {
            case Type::I8:
            case Type::I16:
            case Type::I32:
            case Type::I64:
            case Type::V64:
            case Type::I128:
              llvm_unreachable("invalid constant");
            case Type::F32:
            case Type::F64:
            case Type::F80:
            case Type::F128: {
              const auto &f = static_cast<ConstantFloat &>(c).GetValue();
              Mark(inst, SCCPEval::Extend(Lattice::CreateFloat(f), ty));
              return;
            }
          }
          llvm_unreachable("invalid type");
          break;
        }
      }
      llvm_unreachable("invalid constant");
    }
  }
  llvm_unreachable("invalid value");
}

// -----------------------------------------------------------------------------
void SCCPSolver::VisitUndefInst(UndefInst &inst)
{
  Mark(inst, Lattice::Undefined());
}

// -----------------------------------------------------------------------------
void SCCPSolver::VisitPhiInst(PhiInst &inst)
{
  if (GetValue(inst).IsOverdefined()) {
    return;
  }

  Type ty = inst.GetType();
  Lattice phiValue = Lattice::Unknown();
  for (unsigned i = 0; i < inst.GetNumIncoming(); ++i) {
    auto *block = inst.GetBlock(i);
    if (!edges_.count({block, inst.getParent()})) {
      continue;
    }
    phiValue = phiValue.LUB(GetValue(inst.GetValue(i)));
  }
  Mark(&inst, phiValue);
}

// -----------------------------------------------------------------------------
void SCCPSolver::VisitInst(Inst &inst)
{
  MarkOverdefined(inst);
}
