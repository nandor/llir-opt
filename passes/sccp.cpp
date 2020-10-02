// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <set>
#include <llvm/ADT/SmallPtrSet.h>
#include "core/block.h"
#include "core/cast.h"
#include "core/constant.h"
#include "core/func.h"
#include "core/prog.h"
#include "core/insts.h"
#include "passes/sccp.h"
#include "passes/sccp/lattice.h"
#include "passes/sccp/eval.h"



// -----------------------------------------------------------------------------
const char *SCCPPass::kPassID = "sccp";

// -----------------------------------------------------------------------------
class SCCPSolver {
public:
  /// Simplifies a function.
  void Solve(Func *func);

  /// Returns a lattice value.
  Lattice &GetValue(Inst *inst);
  /// Returns a lattice value.
  Lattice FromValue(Value *inst, Type ty);

private:
  /// Visits an instruction.
  void Visit(Inst *inst);
  /// Visits a unary instruction.
  void Visit(UnaryInst *inst);
  /// Visits a binary instruction.
  void Visit(BinaryInst *inst);

  /// Visits a block.
  void Visit(Block *block);

  /// Marks a block as executable.
  bool MarkBlock(Block *block);
  /// Marks an edge as executable.
  bool MarkEdge(Inst *inst, Block *to);

  /// Helper for Phi nodes.
  void Phi(PhiInst *inst);

  /// Marks an instruction as overdefined.
  void MarkOverdefined(Inst *inst)
  {
    Mark(inst, Lattice::Overdefined());
  }

  /// Marks an instruction as a constant integer.
  void Mark(Inst *inst, const Lattice &value);

  /// Propagates values from blocks reached by undef conditions.
  bool Propagate(Func *func);

private:
  /// Worklist for overdefined values.
  llvm::SmallVector<Inst *, 64> bottomList_;
  /// Worklist for blocks.
  llvm::SmallVector<Block *, 64> blockList_;
  /// Worklist for instructions.
  llvm::SmallVector<Inst *, 64> instList_;

  /// Mapping from instructions to values.
  std::unordered_map<Inst *, Lattice> values_;
  /// Set of known edges.
  std::set<std::pair<Block *, Block *>> edges_;
  /// Set of executable blocks.
  std::set<const Block *> executable_;
};

// -----------------------------------------------------------------------------
void SCCPSolver::Solve(Func *func)
{
  MarkBlock(&func->getEntryBlock());
  do {
    while (!bottomList_.empty() || !blockList_.empty() || !instList_.empty()) {
      while (!bottomList_.empty()) {
        Visit(bottomList_.pop_back_val());
      }
      while (!instList_.empty()) {
        Visit(instList_.pop_back_val());
      }
      while (!blockList_.empty()) {
        Visit(blockList_.pop_back_val());
      }
    }
  } while (Propagate(func));
}

// -----------------------------------------------------------------------------
void SCCPSolver::Visit(Inst *inst)
{
  if (GetValue(inst).IsOverdefined()) {
    return;
  }
  assert(executable_.count(inst->getParent()));

  auto *block = inst->getParent();
  auto *func = block->getParent();
  switch (inst->GetKind()) {
    // Instructions with no successors and void instructions.
    case Inst::Kind::TCALL:
    case Inst::Kind::RET:
    case Inst::Kind::RAISE:
    case Inst::Kind::TRAP:
    case Inst::Kind::FNSTCW:
    case Inst::Kind::FLDCW:
    case Inst::Kind::VASTART: {
      return;
    }

    // Overdefined instructions.
    case Inst::Kind::CALL:
    case Inst::Kind::ST:
    case Inst::Kind::ARG:
    case Inst::Kind::XCHG:
    case Inst::Kind::CMPXCHG:
    case Inst::Kind::SYSCALL:
    case Inst::Kind::ALLOCA:
    case Inst::Kind::RDTSC: {
      MarkOverdefined(inst);
      return;
    }
    // Loads can propagate undefined values.
    case Inst::Kind::LD: {
      auto *ldInst = static_cast<LoadInst *>(inst);
      if (GetValue(ldInst->GetAddr()).IsUndefined()) {
        Mark(inst, Lattice::Undefined());
      } else {
        MarkOverdefined(ldInst);
      }
      return;
    }

    // Control flow.
    case Inst::Kind::INVOKE: {
      auto *invokeInst = static_cast<InvokeInst *>(inst);
      MarkEdge(invokeInst, invokeInst->GetCont());
      MarkEdge(invokeInst, invokeInst->GetThrow());
      MarkOverdefined(invokeInst);
      return;
    }

    case Inst::Kind::TINVOKE: {
      auto *invokeInst = static_cast<TailInvokeInst *>(inst);
      MarkEdge(invokeInst, invokeInst->GetThrow());
      return;
    }

    case Inst::Kind::JCC: {
      auto *jccInst = static_cast<JumpCondInst *>(inst);
      auto &val = GetValue(jccInst->GetCond());
      if (val.IsUnknown()) {
        return;
      }

      if (val.IsTrue() || val.IsOverdefined()) {
        MarkEdge(jccInst, jccInst->GetTrueTarget());
      }
      if (val.IsFalse() || val.IsOverdefined()) {
        MarkEdge(jccInst, jccInst->GetFalseTarget());
      }
      return;
    }

    case Inst::Kind::JMP: {
      auto *jmpInst = static_cast<JumpInst *>(inst);
      MarkEdge(jmpInst, jmpInst->GetTarget());
      return;
    }

    case Inst::Kind::SWITCH: {
      auto *switchInst = static_cast<SwitchInst *>(inst);
      auto &val = GetValue(switchInst->GetIdx());
      if (val.IsUnknown()) {
        return;
      }

      if (auto intVal = val.AsInt()) {
        auto index = intVal->getSExtValue();
        if (index < switchInst->getNumSuccessors()) {
          MarkEdge(switchInst, switchInst->getSuccessor(index));
        }
      } else if (val.IsOverdefined()) {
        for (unsigned i = 0; i < switchInst->getNumSuccessors(); ++i) {
          MarkEdge(switchInst, switchInst->getSuccessor(i));
        }
      }
      return;
    }

    // Ternary operator - propagate a value or select undef.
    case Inst::Kind::SELECT: {
      auto *selectInst = static_cast<SelectInst *>(inst);
      auto &cond = GetValue(selectInst->GetCond());
      auto &valTrue = GetValue(selectInst->GetTrue());
      auto &valFalse = GetValue(selectInst->GetFalse());
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
        MarkOverdefined(inst);
      }
      return;
    }

    // Constant instructions.
    case Inst::Kind::FRAME: {
      auto *fi = static_cast<FrameInst *>(inst);
      Mark(inst, Lattice::CreateFrame(fi->GetObject(), fi->GetOffset()));
      break;
    }
    case Inst::Kind::MOV: {
      auto *movInst = static_cast<MovInst *>(inst);
      Mark(inst, FromValue(movInst->GetArg(), movInst->GetType()));
      return;
    }

    // Undefined.
    case Inst::Kind::UNDEF: {
      Mark(inst, Lattice::Undefined());
      return;
    }

    // PHI nodes.
    case Inst::Kind::PHI: {
      Phi(static_cast<PhiInst *>(inst));
      return;
    }

    // Unary instructions.
    case Inst::Kind::ABS:
    case Inst::Kind::NEG:
    case Inst::Kind::SQRT:
    case Inst::Kind::SIN:
    case Inst::Kind::COS:
    case Inst::Kind::SEXT:
    case Inst::Kind::ZEXT:
    case Inst::Kind::FEXT:
    case Inst::Kind::XEXT:
    case Inst::Kind::TRUNC:
    case Inst::Kind::EXP:
    case Inst::Kind::EXP2:
    case Inst::Kind::LOG:
    case Inst::Kind::LOG2:
    case Inst::Kind::LOG10:
    case Inst::Kind::FCEIL:
    case Inst::Kind::FFLOOR:
    case Inst::Kind::POPCNT:
    case Inst::Kind::CLZ:
    case Inst::Kind::CTZ: {
      auto *unaryInst = static_cast<UnaryInst *>(inst);
      auto &argVal = GetValue(unaryInst->GetArg());
      if (argVal.IsUnknown()) {
        return;
      }

      Mark(inst, SCCPEval::Eval(unaryInst, argVal));
      return;
    }

    // Binary instructions.
    case Inst::Kind::ADD:
    case Inst::Kind::AND:
    case Inst::Kind::CMP:
    case Inst::Kind::UDIV:
    case Inst::Kind::SDIV:
    case Inst::Kind::UREM:
    case Inst::Kind::SREM:
    case Inst::Kind::MUL:
    case Inst::Kind::OR:
    case Inst::Kind::ROTL:
    case Inst::Kind::ROTR:
    case Inst::Kind::SLL:
    case Inst::Kind::SRA:
    case Inst::Kind::SRL:
    case Inst::Kind::SUB:
    case Inst::Kind::XOR:
    case Inst::Kind::POW:
    case Inst::Kind::COPYSIGN:
    case Inst::Kind::SADDO:
    case Inst::Kind::SMULO:
    case Inst::Kind::SSUBO:
    case Inst::Kind::UADDO:
    case Inst::Kind::UMULO:
    case Inst::Kind::USUBO: {
      auto *binaryInst = static_cast<BinaryInst *>(inst);
      auto &lhsVal = GetValue(binaryInst->GetLHS());
      auto &rhsVal = GetValue(binaryInst->GetRHS());
      if (lhsVal.IsUnknown() || rhsVal.IsUnknown()) {
        return;
      }

      Mark(inst, SCCPEval::Eval(binaryInst, lhsVal, rhsVal));
      return;
    }
  }
}

// -----------------------------------------------------------------------------
void SCCPSolver::Visit(Block *block)
{
  for (auto &inst : *block) {
    Visit(&inst);
  }
}

// -----------------------------------------------------------------------------
void SCCPSolver::Mark(Inst *inst, const Lattice &newValue)
{
  auto &oldValue = GetValue(inst);
  if (oldValue == newValue) {
    return;
  }

  oldValue = newValue;
  for (auto *user : inst->users()) {
    assert(user->Is(Value::Kind::INST));
    auto *inst = static_cast<Inst *>(user);

    // If inst not yet executable, do not queue.
    if (!executable_.count(inst->getParent())) {
      continue;
    }
    // If inst is already overdefined, no need to queue.
    if (GetValue(inst).IsOverdefined()) {
      continue;
    }
    // Propagate overdefined sooner.
    if (newValue.IsOverdefined()) {
      bottomList_.push_back(inst);
    } else {
      instList_.push_back(inst);
    }
  }
}

// -----------------------------------------------------------------------------
bool SCCPSolver::MarkEdge(Inst *inst, Block *to)
{
  Block *from = inst->getParent();

  // If the edge was marked previously, do nothing.
  if (!edges_.insert({ from, to }).second) {
    return false;
  }

  // If the block was not executable, revisit PHIs.
  if (!MarkBlock(to)) {
    for (PhiInst &phi : to->phis()) {
      Phi(&phi);
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
  blockList_.push_back(block);
  return true;
}

// -----------------------------------------------------------------------------
void SCCPSolver::Phi(PhiInst *inst)
{
  if (GetValue(inst).IsOverdefined()) {
    return;
  }

  Type ty = inst->GetType();
  Lattice phiValue = Lattice::Unknown();
  for (unsigned i = 0; i < inst->GetNumIncoming(); ++i) {
    auto *block = inst->GetBlock(i);
    if (!edges_.count({block, inst->getParent()})) {
      continue;
    }

    const auto &value = FromValue(inst->GetValue(i), ty);
    if (value.IsUnknown()) {
      continue;
    }

    if (phiValue.IsUnknown()) {
      phiValue = SCCPEval::Extend(value, ty);
    } else if (phiValue != value) {
      phiValue = Lattice::Overdefined();
    }
  }
  Mark(inst, phiValue);
}

// -----------------------------------------------------------------------------
Lattice &SCCPSolver::GetValue(Inst *inst)
{
  return values_.emplace(inst, Lattice::Unknown()).first->second;
}

// -----------------------------------------------------------------------------
Lattice SCCPSolver::FromValue(Value *value, Type ty)
{
  switch (value->GetKind()) {
    case Value::Kind::INST: {
      return SCCPEval::Bitcast(GetValue(static_cast<Inst *>(value)), ty);
    }
    case Value::Kind::GLOBAL: {
      return Lattice::CreateGlobal(static_cast<Global *>(value));
    }
    case Value::Kind::EXPR: {
      switch (static_cast<Expr *>(value)->GetKind()) {
        case Expr::Kind::SYMBOL_OFFSET: {
          auto *sym = static_cast<SymbolOffsetExpr *>(value);
          return Lattice::CreateGlobal(sym->GetSymbol(), sym->GetOffset());
        }
      }
      llvm_unreachable("invalid expression");
    }
    case Value::Kind::CONST: {
      union U { int64_t i; double d; };
      switch (static_cast<Constant *>(value)->GetKind()) {
        case Constant::Kind::INT: {
          const auto &i = static_cast<ConstantInt *>(value)->GetValue();
          switch (ty) {
            case Type::I8:
            case Type::I16:
            case Type::I32:
            case Type::I64:
            case Type::I128:
              return SCCPEval::Extend(Lattice::CreateInteger(i), ty);
            case Type::F32:
            case Type::F64:
            case Type::F80: {
              APFloat f((U{ .i = i.getSExtValue() }).d);
              return SCCPEval::Extend(Lattice::CreateFloat(f), ty);
            }
          }
          llvm_unreachable("invalid type");
          break;
        }
        case Constant::Kind::FLOAT: {
          const auto &f = static_cast<ConstantFloat *>(value)->GetValue();
          switch (ty) {
            case Type::I8:
            case Type::I16:
            case Type::I32:
            case Type::I64:
            case Type::I128:
              llvm_unreachable("invalid constant");
            case Type::F32:
            case Type::F64:
            case Type::F80: {
              const auto &f = static_cast<ConstantFloat *>(value)->GetValue();
              return SCCPEval::Extend(Lattice::CreateFloat(f), ty);
            }
          }
          llvm_unreachable("invalid type");
          break;
        }
        case Constant::Kind::REG: {
          return Lattice::Overdefined();
        }
      }
      llvm_unreachable("invalid constant");
    }
  }
  llvm_unreachable("invalid value");
}

// -----------------------------------------------------------------------------
bool SCCPSolver::Propagate(Func *func)
{
  for (auto &block : *func) {
    if (!executable_.count(&block)) {
      continue;
    }
    for (Inst &inst : block) {
      // If the jump's condition was undefined, select a branch.
      if (auto *jccInst = ::dyn_cast_or_null<JumpCondInst>(&inst)) {
        auto &val = GetValue(jccInst->GetCond());
        if (val.IsTrue() || val.IsFalse() || val.IsOverdefined()) {
          continue;
        }

        return MarkEdge(jccInst, jccInst->GetFalseTarget());
      }

      // If the switch was undefined or out of range, select a branch.
      if (auto *switchInst = ::dyn_cast_or_null<SwitchInst>(&inst)) {
        auto &val = GetValue(switchInst->GetIdx());
        auto numBranches = switchInst->getNumSuccessors();
        if (auto i = val.AsInt(); i && i->getZExtValue() < numBranches) {
          continue;
        }
        if (val.IsOverdefined()) {
          continue;
        }

        return MarkEdge(switchInst, switchInst->getSuccessor(0));
      }
    }
  }

  return false;
}

// -----------------------------------------------------------------------------
void SCCPPass::Run(Prog *prog)
{
  for (auto &func : *prog) {
    Run(func);
  }
}

// -----------------------------------------------------------------------------
void SCCPPass::Run(Func &func)
{
  SCCPSolver solver;
  solver.Solve(&func);

  for (auto &block : func) {
    Inst *firstNonPhi = nullptr;
    auto GetInsertPoint = [&firstNonPhi](Inst *inst) {
      if (!inst->Is(Inst::Kind::PHI)) {
        return inst;
      }
      if (!firstNonPhi) {
        firstNonPhi = inst;
        while (firstNonPhi->Is(Inst::Kind::PHI)) {
          firstNonPhi = &*std::next(firstNonPhi->getIterator());
        }
      }
      return firstNonPhi;
    };
    for (auto it = block.begin(); it != block.end(); ) {
      Inst *inst = &*it++;

      // Some instructions are not mapped to values.
      if (inst->IsVoid() || inst->IsConstant()) {
        continue;
      }

      // Find the relevant info from the original instruction.
      auto type = inst->GetType(0);
      const auto &v = solver.GetValue(inst);
      const auto &annot = inst->GetAnnots();

      // Create a constant integer.
      Inst *newInst = nullptr;
      switch (v.GetKind()) {
        case Lattice::Kind::UNKNOWN:
        case Lattice::Kind::OVERDEFINED: {
          continue;
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
              new ConstantInt(v.GetFrameObject()),
              new ConstantInt(v.GetFrameOffset()),
              annot
          );
          break;
        }
        case Lattice::Kind::GLOBAL: {
          Value *global = nullptr;
          Global *sym = v.GetGlobalSymbol();
          if (auto offset = v.GetGlobalOffset()) {
            global = new SymbolOffsetExpr(sym, offset);
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

      block.AddInst(newInst, GetInsertPoint(inst));
      inst->replaceAllUsesWith(newInst);
      inst->eraseFromParent();
    }
  }
}

// -----------------------------------------------------------------------------
const char *SCCPPass::GetPassName() const
{
  return "Sparse Conditional Constant Propagation";
}
