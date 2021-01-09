// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <set>
#include <queue>

#include <llvm/ADT/SmallPtrSet.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/constant.h"
#include "core/func.h"
#include "core/prog.h"
#include "core/insts.h"
#include "core/pass_manager.h"
#include "passes/sccp.h"
#include "passes/sccp/lattice.h"
#include "passes/sccp/eval.h"



// -----------------------------------------------------------------------------
const char *SCCPPass::kPassID = "sccp";

// -----------------------------------------------------------------------------
const char *SCCPPass::GetPassName() const
{
  return "Sparse Conditional Constant Propagation";
}

// -----------------------------------------------------------------------------
class SCCPSolver : InstVisitor<void> {
public:
  /// Solves constraints for the whole program.
  SCCPSolver(Prog &prog);

  /// Returns a lattice value.
  Lattice &GetValue(Ref<Inst> inst);
  /// Checks if a block is executable.
  bool IsExecutable(const Block &block) { return executable_.count(&block); }

private:
  /// Visits a block.
  void Visit(Block *block)
  {
    for (auto &inst : *block) {
      Visit(inst);
    }
  }

  /// Visits an instruction.
  void Visit(Inst &inst)
  {
    assert(executable_.count(inst.getParent()) && "bb not yet visited");
    Dispatch(inst);
  }

  /// Checks if a call can be evaluated.
  bool CanEvaluate(CallSite &site);
  /// Marks a call return as overdefined.
  void MarkOverdefinedCall(TailCallInst &site);
  /// Calls a function with a given set of arguments.
  void MarkCall(CallSite &site, Func &callee, Block *cont);

  /// Marks a block as executable.
  bool MarkBlock(Block *block);
  /// Marks an edge as executable.
  bool MarkEdge(Inst &inst, Block *to);
  /// Marks an instruction as overdefined.
  bool MarkOverdefined(Inst &inst)
  {
    bool changed = false;
    for (unsigned i = 0, n = inst.GetNumRets(); i < n; ++i) {
      changed |= Mark(inst.GetSubValue(i), Lattice::Overdefined());
    }
    return changed;
  }
  /// Marks an instruction as a constant integer.
  bool Mark(Ref<Inst> inst, const Lattice &value);

private:
  void VisitArgInst(ArgInst &inst) override;
  void VisitCallInst(CallInst &inst) override;
  void VisitTailCallInst(TailCallInst &inst) override;
  void VisitInvokeInst(InvokeInst &inst) override;
  void VisitReturnInst(ReturnInst &inst) override;
  void VisitLoadInst(LoadInst &inst) override;
  void VisitUnaryInst(UnaryInst &inst) override;
  void VisitBinaryInst(BinaryInst &inst) override;
  void VisitJumpInst(JumpInst &inst) override;
  void VisitJumpCondInst(JumpCondInst &inst) override;
  void VisitSwitchInst(SwitchInst &inst) override;
  void VisitSelectInst(SelectInst &inst) override;
  void VisitFrameInst(FrameInst &inst) override;
  void VisitMovInst(MovInst &inst) override;
  void VisitUndefInst(UndefInst &inst) override;
  void VisitPhiInst(PhiInst &inst) override;
  void VisitInst(Inst &inst) override;

private:
  /// Worklist for overdefined values.
  std::queue<Inst *> bottomList_;
  /// Worklist for blocks.
  std::queue<Block *> blockList_;
  /// Worklist for instructions.
  std::queue<Inst *> instList_;

  /// Mapping from instructions to values.
  std::unordered_map<Ref<Inst>, Lattice> values_;
  /// Set of known edges.
  std::set<std::pair<Block *, Block *>> edges_;
  /// Set of executable blocks.
  std::set<const Block *> executable_;
  /// Collection of all arguments used by any function.
  std::map<const Func *, std::map<unsigned, std::set<ArgInst *>>> args_;
  /// Call sites which reach a particular function.
  std::map<const Func *, std::set<std::pair<CallSite *, Block *>>> calls_;
  /// Mapping to the return values of a function.
  std::map<const Func *, std::map<unsigned, Lattice>> returns_;
};

// -----------------------------------------------------------------------------
SCCPSolver::SCCPSolver(Prog &prog)
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
          if (auto vt = it->second.find(i); vt != it->second.end()) {
            Mark(ci->GetSubValue(i), SCCPEval::Extend(vt->second, ci->type(i)));
          } else {
            Mark(ci->GetSubValue(i), Lattice::Undefined());
          }
        }
        MarkEdge(*ci, cont);
      } else {
        Func *caller = ci->getParent()->getParent();
        if (!visited.insert(caller).second) {
          continue;
        }

        auto tret = returns_.emplace(caller, std::map<unsigned, Lattice>{});
        auto &rets = tret.first->second;
        if (tret.second || rets != it->second) {
          for (auto [idx, val] : it->second) {
            auto tt = rets.emplace(idx, val);
            if (!tt.second) {
              tt.first->second = val.LUB(tt.first->second);
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
    auto it = returns_.emplace(f, std::map<unsigned, Lattice>{});
    bool changed = it.second;
    auto &rets = it.first->second;
    for (unsigned i = 0, n = inst.type_size(); i < n; ++i) {
      if (auto it = rets.find(i); it != rets.end()) {
        if (!it->second.IsOverdefined()) {
          it->second = Lattice::Overdefined();
          changed = true;
        }
      } else {
        rets.emplace(i, Lattice::Overdefined());
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
              Mark(ref, it->second);
            } else {
              Mark(ref, Lattice::Undefined());
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
  if (val.IsGlobal()) {
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
    auto it = returns_.emplace(f, std::map<unsigned, Lattice>{});
    auto &rets = it.first->second;
    if (it.second) {
      // First time returning - insert the values.
      for (unsigned i = 0, n = inst.arg_size(); i < n; ++i) {
        rets.emplace(i, GetValue(inst.arg(i)));
      }
    } else {
      // Previous returns occurred - consider missing values to be undef.
      // Add the LUB of the newly returned value and the old one or undef.
      for (unsigned i = 0, n = inst.arg_size(); i < n; ++i) {
        const auto value = GetValue(inst.arg(i));
        if (auto it = rets.find(i); it != rets.end()) {
          it->second = value.LUB(it->second);
        } else if (!value.IsUndefined()) {
          rets.emplace(i, value.LUB(Lattice::Undefined()));
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
          if (auto it = rets.find(i); it != rets.end()) {
            Mark(ref, SCCPEval::Extend(it->second, ci->type(i)));
          } else {
            Mark(ref, Lattice::Undefined());
          }
        }
        MarkEdge(*ci, cont);
      } else {
        q.push(ci->getParent()->getParent());
      }
    }
  }
}

// -----------------------------------------------------------------------------
void SCCPSolver::VisitLoadInst(LoadInst &inst)
{
  auto &addrVal = GetValue(inst.GetAddr());
  Mark(inst, SCCPEval::Eval(&inst, addrVal));
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
    if (val.IsTrue() || val.IsOverdefined()) {
      MarkEdge(inst, inst.GetTrueTarget());
    }
    if (val.IsFalse() || val.IsOverdefined()) {
      MarkEdge(inst, inst.GetFalseTarget());
    }
  } else {
    MarkEdge(inst, inst.GetFalseTarget());
  }
}

// -----------------------------------------------------------------------------
void SCCPSolver::VisitSwitchInst(SwitchInst &inst)
{
  auto &val = GetValue(inst.GetIndex());
  if (val.IsUnknown()) {
    return;
  }

  if (auto intVal = val.AsInt()) {
    auto index = intVal->getSExtValue();
    if (index < inst.getNumSuccessors()) {
      MarkEdge(inst, inst.getSuccessor(index));
    }
  } else if (val.IsOverdefined()) {
    for (unsigned i = 0; i < inst.getNumSuccessors(); ++i) {
      MarkEdge(inst, inst.getSuccessor(i));
    }
  } else {
    MarkEdge(inst, inst.getSuccessor(0));
  }
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
            case Type::I128:
              Mark(inst, SCCPEval::Extend(Lattice::CreateInteger(i), ty));
              return;
            case Type::F32: {
              Mark(inst, Lattice::CreateFloat(APFloat(
                  APFloat::IEEEsingle(),
                  i.zextOrTrunc(32)
              )));
              return;
            }
            case Type::F64: {
              Mark(inst, Lattice::CreateFloat(APFloat(
                  APFloat::IEEEdouble(),
                  i.zextOrTrunc(64)
              )));
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

// -----------------------------------------------------------------------------
static bool Rewrite(Func &func, SCCPSolver &solver)
{
  bool changed = false;
  bool removeUnreachable = false;
  for (auto &block : func) {
    if (!solver.IsExecutable(block)) {
      // If the block is not reachable, replace it with a trap.
      if (block.size() > 1 || !block.GetTerminator()->Is(Inst::Kind::TRAP)) {
        std::set<Block *> succs(block.succ_begin(), block.succ_end());
        for (Block *succ : succs) {
          removeUnreachable = true;
          for (auto &phi : succ->phis()) {
            phi.Remove(&block);
          }
        }
        block.clear();
        block.AddInst(new TrapInst({}));
      }
    } else {
      // Replace individual instructions with constants.
      for (auto it = block.begin(); it != block.end(); ) {
        Inst *inst = &*it++;
        // Some instructions are not mapped to values.
        if (inst->IsVoid() || inst->IsConstant() || inst->HasSideEffects()) {
          continue;
        }

        // Find the value assigned to the instruction.
        llvm::SmallVector<Ref<Inst>, 4> newValues;
        unsigned numValues = 0;
        for (unsigned i = 0, n = inst->GetNumRets(); i < n; ++i) {
          auto ref = inst->GetSubValue(i);
          const auto &v = solver.GetValue(ref);

          // Find the relevant info from the original instruction.
          // The type is downgraded from V64 to I64 since constants are
          // not heap roots, thus they do not need to be tracked.
          Type type = ref.GetType() == Type::V64 ? Type::I64 : ref.GetType();
          const auto &annot = inst->GetAnnots();

          // Create a mov instruction producing a constant value.
          Inst *newInst = nullptr;
          switch (v.GetKind()) {
            case Lattice::Kind::UNKNOWN:
            case Lattice::Kind::OVERDEFINED:
            case Lattice::Kind::POINTER:
            case Lattice::Kind::FLOAT_ZERO: {
              break;
            }
            case Lattice::Kind::INT: {
              newInst = new MovInst(type, new ConstantInt(v.GetInt()), annot);
              break;
            }
            case Lattice::Kind::FLOAT: {
              newInst = new MovInst(type, new ConstantFloat(v.GetFloat()), annot);
              break;
            }
            case Lattice::Kind::FRAME: {
              newInst = new FrameInst(
                  type,
                  v.GetFrameObject(),
                  v.GetFrameOffset(),
                  annot
              );
              break;
            }
            case Lattice::Kind::GLOBAL: {
              Value *global = nullptr;
              Global *sym = v.GetGlobalSymbol();
              if (auto offset = v.GetGlobalOffset()) {
                global = SymbolOffsetExpr::Create(sym, offset);
              } else {
                global = sym;
              }
              newInst = new MovInst(type, global, annot);
              break;
            }
            case Lattice::Kind::UNDEFINED: {
              newInst = new UndefInst(type, annot);
              break;
            }
          }

          // Add the new instruction prior to the replaced one. This ensures
          // constant return values are placed before the call instructions
          // producing them.
          if (newInst) {
            auto insert = inst->getIterator();
            while (insert->Is(Inst::Kind::PHI)) {
              ++insert;
            }
            block.AddInst(newInst, &*insert);
            newValues.push_back(newInst);
            numValues++;
            changed = true;
          } else {
            newValues.push_back(ref);
          }
        }

        // Replaces uses if any of them changed and erase the instruction if no
        // users are left, unless the instruction has side effects.
        if (numValues) {
          inst->replaceAllUsesWith(newValues);
          inst->eraseFromParent();
        }
      }
    }
  }
  if (removeUnreachable) {
    func.RemoveUnreachable();
  }
  return changed;
}

// -----------------------------------------------------------------------------
bool SCCPPass::Run(Prog &prog)
{
  SCCPSolver solver(prog);
  bool changed = false;
  for (auto &func : prog) {
    changed = Rewrite(func, solver) || changed;
  }
  return changed;
}
