// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <set>
#include <llvm/ADT/SmallPtrSet.h>
#include "core/block.h"
#include "core/constant.h"
#include "core/func.h"
#include "core/prog.h"
#include "core/insts.h"
#include "passes/sccp.h"



// -----------------------------------------------------------------------------
const char *SCCPPass::kPassID = "sccp";

// -----------------------------------------------------------------------------
class Lattice {
public:
  /// Enumeration of lattice value kinds.
  enum class Kind {
    /// Top - value not encountered yet.
    UNKNOWN,
    /// Constant integer.
    INT,
    /// Constant floating-point.
    FLOAT,
    /// Constant, frame address.
    FRAME,
    /// Constant, symbol with offset.
    GLOBAL,
    /// Constant, undefined.
    UNDEFINED,
    /// Bot - value is not constant.
    OVERDEFINED,
  };

  Lattice(int64_t val) : kind_(Kind::INT), intVal_(val) { }

  Lattice(double val) : kind_(Kind::FLOAT), doubleVal_(val) { }

  Lattice(Global *val, int64_t offset = 0)
    : kind_(Kind::GLOBAL)
    , intVal_(offset)
    , symVal_(val)
  {
  }

  Kind GetKind() const { return kind_; }

  static Lattice Unknown() { return Lattice(Kind::UNKNOWN); }

  static Lattice Overdefined() { return Lattice(Kind::OVERDEFINED); }

  static Lattice Frame(unsigned offset)
  {
    Lattice lattice(Kind::FRAME);
    lattice.intVal_ = offset;
    return lattice;
  }

  bool IsInt() const { return kind_ == Kind::INT; }
  bool IsFrame() const { return kind_ == Kind::FRAME; }
  bool IsGlobal() const { return kind_ == Kind::GLOBAL; }
  bool IsOverdefined() const { return kind_ == Kind::OVERDEFINED; }

  int64_t GetInt() const { assert(IsInt()); return intVal_; }
  unsigned GetFrame() const { assert(IsFrame()); return intVal_; }
  Global *GetGlobal() const { assert(IsGlobal()); return symVal_; }
  int64_t GetOffset() const { assert(IsGlobal()); return intVal_; }

  bool IsConstant() const
  {
    switch (kind_) {
      case Kind::UNKNOWN:     return false;
      case Kind::INT:         return true;
      case Kind::FLOAT:       return true;
      case Kind::FRAME:       return true;
      case Kind::GLOBAL:      return true;
      case Kind::UNDEFINED:   return true;
      case Kind::OVERDEFINED: return false;
    }
  }

  bool IsTrue() const
  {
    switch (kind_) {
      case Kind::UNKNOWN:     assert(!"invalid lattice value");
      case Kind::INT:         return intVal_ != 0;
      case Kind::FLOAT:       return doubleVal_ != 0.0;
      case Kind::FRAME:       return true;
      case Kind::GLOBAL:      return true;
      case Kind::UNDEFINED:   return false;
      case Kind::OVERDEFINED: return false;
    }
  }

  bool IsFalse() const
  {
    switch (kind_) {
      case Kind::UNKNOWN:     assert(!"invalid lattice value");
      case Kind::INT:         return intVal_ == 0;
      case Kind::FLOAT:       return doubleVal_ == 0.0;
      case Kind::FRAME:       return false;
      case Kind::GLOBAL:      return false;
      case Kind::UNDEFINED:   return true;
      case Kind::OVERDEFINED: return false;
    }
  }

  bool operator != (const Lattice &o) const
  {
    if (kind_ != o.kind_) {
      return true;
    }

    switch (kind_) {
      case Kind::UNKNOWN: return true;
      case Kind::INT: return intVal_ != o.intVal_;
      case Kind::FLOAT: return doubleVal_ != o.doubleVal_;
      case Kind::FRAME: return intVal_ != o.intVal_;
      case Kind::GLOBAL: return intVal_ != o.intVal_ || symVal_ != o.symVal_;
      case Kind::UNDEFINED: return false;
      case Kind::OVERDEFINED: return true;
    }
  }

private:
  /// Private constructor for special values.
  Lattice(Kind kind) : kind_(kind) {}

  /// Kind of the lattice value.
  Kind kind_;
  /// Union of possible values.
  union {
    /// Integer value.
    int64_t intVal_;
    /// Double value.
    double doubleVal_;
  };
  /// Global value.
  Global *symVal_;
};



// -----------------------------------------------------------------------------
class SCCPSolver {
public:

  /// Simplifies a function.
  void Run(Func *func);

  /// Returns a lattice value.
  Lattice &GetValue(Inst *inst);
  /// Returns a lattice value.
  Lattice GetValue(Value *inst);

private:
  /// Visits an instruction.
  void Visit(Inst *inst);
  /// Visits a block.
  void Visit(Block *block);

  /// Marks a block as executable.
  bool MarkBlock(Block *block);
  /// Marks an edge as executable.
  void MarkEdge(Inst *inst, Block *to);

  /// Helper for unary instructions.
  void Unary(Inst *inst, std::function<Lattice(Lattice &)> &&func);
  /// Helper for binary instructions.
  void Binary(Inst *inst, std::function<Lattice(Lattice &, Lattice &)> &&func);
  /// Helper for Phi nodes.
  void Phi(PhiInst *inst);

  /// Marks an instruction as overdefined.
  void MarkOverdefined(Inst *inst)
  {
    Mark(inst, Lattice::Overdefined());
  }

  /// Marks an instruction as a constant integer.
  void Mark(Inst *inst, const Lattice &value);

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
  std::set<Block *> executable_;
};

// -----------------------------------------------------------------------------
void SCCPSolver::Run(Func *func)
{
  MarkBlock(&func->getEntryBlock());
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
}

// -----------------------------------------------------------------------------
void SCCPSolver::Visit(Inst *inst)
{
  if (GetValue(inst).IsOverdefined()) {
    return;
  }

  auto *block = inst->getParent();
  auto *func = block->getParent();

  switch (inst->GetKind()) {
    // Instructions with no successors and void instructions.
    case Inst::Kind::TCALL:   return;
    case Inst::Kind::RET:     return;
    case Inst::Kind::JI:      return;
    case Inst::Kind::TRAP:    return;
    case Inst::Kind::SET:     return;
    case Inst::Kind::VASTART: return;
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
      MarkOverdefined(invokeInst);
      return;
    }
    case Inst::Kind::JCC: {
      auto *jccInst = static_cast<JumpCondInst *>(inst);
      auto &val = GetValue(jccInst->GetCond());
      if (val.IsTrue()) {
        MarkEdge(jccInst, jccInst->GetTrueTarget());
      } else if (val.IsFalse()) {
        MarkEdge(jccInst, jccInst->GetFalseTarget());
      } else {
        MarkEdge(jccInst, jccInst->GetTrueTarget());
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
      if (val.IsConstant()) {
        assert(!"not implemented");
      } else {
        for (unsigned i = 0; i < switchInst->getNumSuccessors(); ++i) {
          MarkEdge(switchInst, switchInst->getSuccessor(i));
        }
      }
      return;
    }
    // Overdefined instructions.
    case Inst::Kind::CALL:   return MarkOverdefined(inst);
    case Inst::Kind::LD:     return MarkOverdefined(inst);
    case Inst::Kind::ST:     return MarkOverdefined(inst);
    case Inst::Kind::ARG:    return MarkOverdefined(inst);
    case Inst::Kind::XCHG:   return MarkOverdefined(inst);
    case Inst::Kind::ALLOCA: return MarkOverdefined(inst);
    // Constant instructions.
    case Inst::Kind::FRAME: {
      Mark(inst, Lattice::Frame(static_cast<FrameInst *>(inst)->GetIdx()));
      break;
    }
    case Inst::Kind::MOV: {
      auto *movInst = static_cast<MovInst *>(inst);
      Mark(inst, GetValue(movInst->GetArg()));
      return;
    }
    // Ternary operators.
    case Inst::Kind::SELECT: {
      auto *selectInst = static_cast<SelectInst *>(inst);

      auto &cond = GetValue(selectInst->GetCond());
      auto &valTrue = GetValue(selectInst->GetTrue());
      auto &valFalse = GetValue(selectInst->GetFalse());

      if (cond.IsTrue() && valTrue.IsConstant()) {
        Mark(inst, valTrue);
      } else if (cond.IsFalse() && valFalse.IsConstant()) {
        Mark(inst, valFalse);
      } else {
        MarkOverdefined(inst);
      }
      return;
    }
    // Unary operators.
    case Inst::Kind::ABS: return Unary(inst, [&, this](auto &arg) {
      return Lattice::Overdefined();
    });
    case Inst::Kind::NEG: return Unary(inst, [&, this](auto &arg) {
      return Lattice::Overdefined();
    });
    case Inst::Kind::SQRT: return Unary(inst, [&, this](auto &arg) {
      return Lattice::Overdefined();
    });
    case Inst::Kind::SIN: return Unary(inst, [&, this](auto &arg) {
      return Lattice::Overdefined();
    });
    case Inst::Kind::COS: return Unary(inst, [&, this](auto &arg) {
      return Lattice::Overdefined();
    });
    case Inst::Kind::SEXT: return Unary(inst, [&, this](auto &arg) {
      return Lattice::Overdefined();
    });
    case Inst::Kind::ZEXT: return Unary(inst, [&, this](auto &arg) {
      return Lattice::Overdefined();
    });
    case Inst::Kind::FEXT: return Unary(inst, [&, this](auto &arg) {
      return Lattice::Overdefined();
    });
    case Inst::Kind::TRUNC: return Unary(inst, [&, this](auto &arg) {
      return Lattice::Overdefined();
    });
    // Binary operators.
    case Inst::Kind::CMP: return Binary(inst, [&, this] (auto &lhs, auto &rhs) {
      return Lattice::Overdefined();
    });
    case Inst::Kind::DIV: return Binary(inst, [&, this] (auto &lhs, auto &rhs) {
      return Lattice::Overdefined();
    });
    case Inst::Kind::REM: return Binary(inst, [&, this] (auto &lhs, auto &rhs) {
      return Lattice::Overdefined();
    });
    case Inst::Kind::MUL: return Binary(inst, [&, this] (auto &lhs, auto &rhs) {
      return Lattice::Overdefined();
    });
    case Inst::Kind::ADD: return Binary(inst, [&, this] (auto &lhs, auto &rhs) {
      switch (inst->GetType(0)) {
        case Type::I8: assert(!"not implemented");
        case Type::I16: assert(!"not implemented");
        case Type::I32: {
          if (lhs.IsInt() && rhs.IsInt()) {
            auto lint = static_cast<int32_t>(lhs.GetInt());
            auto rint = static_cast<int32_t>(rhs.GetInt());
            return Lattice{ static_cast<int64_t>(lint + rint) };
          }
          assert(!"not implemented");
        }
        case Type::I64: {
          if (lhs.IsInt() && rhs.IsInt()) {
            return Lattice{ lhs.GetInt() + rhs.GetInt() };
          }
          if (lhs.IsFrame() && rhs.IsInt()) {
            return Lattice::Frame(lhs.GetFrame() + rhs.GetInt());
          }
          if (lhs.IsGlobal() && rhs.IsInt()) {
            return Lattice(lhs.GetGlobal(), lhs.GetOffset() + rhs.GetInt());
          }
          if (lhs.IsInt() && rhs.IsGlobal()) {
            return Lattice(rhs.GetGlobal(), rhs.GetOffset() + lhs.GetInt());
          }
          assert(!"not implemented");
        }
        case Type::I128: assert(!"not implemented");
        case Type::U8: assert(!"not implemented");
        case Type::U16: assert(!"not implemented");
        case Type::U32: assert(!"not implemented");
        case Type::U64: assert(!"not implemented");
        case Type::U128: assert(!"not implemented");
        case Type::F32: assert(!"not implemented");
        case Type::F64: assert(!"not implemented");
      }
      return Lattice::Overdefined();
    });
    case Inst::Kind::SUB: return Binary(inst, [&, this] (auto &lhs, auto &rhs) {
      return Lattice::Overdefined();
    });
    case Inst::Kind::AND: return Binary(inst, [&, this] (auto &lhs, auto &rhs) {
      return Lattice::Overdefined();
    });
    case Inst::Kind::OR: return Binary(inst, [&, this] (auto &lhs, auto &rhs) {
      return Lattice::Overdefined();
    });
    case Inst::Kind::SLL: return Binary(inst, [&, this] (auto &lhs, auto &rhs) {
      return Lattice::Overdefined();
    });
    case Inst::Kind::SRA: return Binary(inst, [&, this] (auto &lhs, auto &rhs) {
      return Lattice::Overdefined();
    });
    case Inst::Kind::SRL: return Binary(inst, [&, this] (auto &lhs, auto &rhs) {
      return Lattice::Overdefined();
    });
    case Inst::Kind::XOR: return Binary(inst, [&, this] (auto &lhs, auto &rhs) {
      if (lhs.IsInt() && rhs.IsInt()) {
        return Lattice{ lhs.GetInt() ^ rhs.GetInt() };
      }
      return Lattice::Overdefined();
    });
    case Inst::Kind::ROTL: return Binary(inst, [&, this] (auto &lhs, auto &rhs) {
      return Lattice::Overdefined();
    });
    case Inst::Kind::POW: return Binary(inst, [&, this] (auto &lhs, auto &rhs) {
      return Lattice::Overdefined();
    });
    case Inst::Kind::COPYSIGN: return Binary(inst, [&, this] (auto &lhs, auto &rhs) {
      return Lattice::Overdefined();
    });
    case Inst::Kind::UADDO: return Binary(inst, [&, this] (auto &lhs, auto &rhs) {
      return Lattice::Overdefined();
    });
    case Inst::Kind::UMULO: return Binary(inst, [&, this] (auto &lhs, auto &rhs) {
      return Lattice::Overdefined();
    });
    // Undefined.
    case Inst::Kind::UNDEF: {
      Mark(inst, Lattice::Overdefined());
      return;
    }
    // PHI nodes.
    case Inst::Kind::PHI: return Phi(static_cast<PhiInst *>(inst));
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
void SCCPSolver::Mark(Inst *inst, const Lattice &value)
{
  auto &oldValue = GetValue(inst);
  if (oldValue.IsConstant() && value.IsConstant()) {
    return;
  }
  if (oldValue.IsOverdefined() && value.IsOverdefined()) {
    return;
  }

  oldValue = value;
  for (auto *user : inst->users()) {
    assert(user->Is(Value::Kind::INST));
    auto *inst = static_cast<Inst *>(user);

    if (value.IsOverdefined()) {
      bottomList_.push_back(inst);
    } else {
      instList_.push_back(inst);
    }
  }
}

// -----------------------------------------------------------------------------
void SCCPSolver::MarkEdge(Inst *inst, Block *to)
{
  Block *from = inst->getParent();

  // If the edge was marked previously, do nothing.
  if (!edges_.insert({ from, to }).second) {
    return;
  }

  // If the block was not executable, revisit PHIs.
  if (!MarkBlock(to)) {
    for (PhiInst &phi : to->phis()) {
      Phi(&phi);
    }
  }
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
void SCCPSolver::Unary(Inst *inst, std::function<Lattice(Lattice &)> &&func)
{
  auto *unaryInst = static_cast<UnaryInst *>(inst);
  auto &arg = GetValue(unaryInst->GetArg());

  if (arg.IsConstant()) {
    Mark(inst, func(arg));
    return;
  }

  MarkOverdefined(inst);
}

// -----------------------------------------------------------------------------
void SCCPSolver::Binary(
    Inst *inst,
    std::function<Lattice(Lattice &, Lattice &)> &&func)
{
  auto *binInst = static_cast<BinaryInst *>(inst);
  auto &lhs = GetValue(binInst->GetLHS());
  auto &rhs = GetValue(binInst->GetRHS());

  if (lhs.IsConstant() && rhs.IsConstant()) {
    Mark(inst, func(lhs, rhs));
    return;
  }

  MarkOverdefined(inst);
}

// -----------------------------------------------------------------------------
void SCCPSolver::Phi(PhiInst *inst)
{
  if (GetValue(inst).IsOverdefined()) {
    return;
  }

  bool hasPhi = false;
  Lattice phiValue = Lattice::Overdefined();
  for (unsigned i = 0; i < inst->GetNumIncoming(); ++i) {
    auto *block = inst->GetBlock(i);
    if (!edges_.count({block, inst->getParent()})) {
      continue;
    }

    auto *value = inst->GetValue(i);
    const auto &lattice = GetValue(value);
    if (!hasPhi) {
      phiValue = lattice;
      hasPhi = true;
    } else if (phiValue != lattice) {
      phiValue = Lattice::Overdefined();
      break;
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
Lattice SCCPSolver::GetValue(Value *value)
{
  switch (value->GetKind()) {
    case Value::Kind::INST: {
      return GetValue(static_cast<Inst *>(value));
    }
    case Value::Kind::GLOBAL: {
      return static_cast<Global *>(value);
    }
    case Value::Kind::EXPR: {
      switch (static_cast<Expr *>(value)->GetKind()) {
        case Expr::Kind::SYMBOL_OFFSET: {
          auto *symExpr = static_cast<SymbolOffsetExpr *>(value);
          return Lattice{ symExpr->GetSymbol(), symExpr->GetOffset() };
        }
      }
      return Lattice::Overdefined();
    }
    case Value::Kind::CONST: {
      switch (static_cast<Constant *>(value)->GetKind()) {
        case Constant::Kind::INT: {
          return static_cast<ConstantInt *>(value)->GetValue();
        }
        case Constant::Kind::FLOAT: {
          return static_cast<ConstantFloat *>(value)->GetValue();
        }
        case Constant::Kind::REG: {
          return Lattice::Overdefined();
        }
      }
    }
  }
}

// -----------------------------------------------------------------------------
void SCCPPass::Run(Prog *prog)
{
  for (auto &func : *prog) {
    SCCPSolver solver;
    solver.Run(&func);

    for (auto &block : func) {
      for (auto it = block.begin(); it != block.end(); ) {
        Inst *inst = &*it++;
        // Cannot be a constant.
        if (inst->IsVoid() || inst->IsConstant()) {
          continue;
        }

        auto type = inst->GetType(0);
        const auto &val = solver.GetValue(inst);
        const auto &annot = inst->GetAnnot();

        // Not a constant or already a mode.
        if (!val.IsConstant()) {
          continue;
        }

        // Create a constant integer.
        Inst *newInst = nullptr;
        if (val.IsInt()) {
          newInst = new MovInst(type, new ConstantInt(val.GetInt()), annot);
        } else if (val.IsFrame()) {
          newInst = new FrameInst(type, new ConstantInt(val.GetFrame()), annot);
        } else if (val.IsGlobal()) {
          Value *global = nullptr;
          if (auto offset = val.GetOffset()) {
            global = prog->CreateSymbolOffset(val.GetGlobal(), offset);
          } else {
            global = val.GetGlobal();
          }
          newInst = new MovInst(type, global, annot);
        } else {
          assert(!"not implemented");
        }

        block.AddInst(newInst, inst);
        inst->replaceAllUsesWith(newInst);
        inst->eraseFromParent();
      }
    }
  }
}

// -----------------------------------------------------------------------------
const char *SCCPPass::GetPassName() const
{
  return "Sparse Conditional Constant Propagation";
}
