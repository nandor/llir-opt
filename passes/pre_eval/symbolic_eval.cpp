// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/Support/Debug.h>

#include "core/expr.h"

#include "passes/pre_eval/symbolic_context.h"
#include "passes/pre_eval/symbolic_eval.h"
#include "passes/pre_eval/symbolic_value.h"
#include "passes/pre_eval/symbolic_visitor.h"
#include "passes/pre_eval/symbolic_heap.h"

#define DEBUG_TYPE "pre-eval"


// -----------------------------------------------------------------------------
bool SymbolicEval::Evaluate(Inst &inst)
{
  LLVM_DEBUG(llvm::dbgs() << inst << '\n');

  bool changed = Dispatch(inst);

  #ifndef NDEBUG
    for (unsigned i = 0, n = inst.GetNumRets(); i < n; ++i) {
      if (auto value = ctx_.FindOpt(inst.GetSubValue(i))) {
        LLVM_DEBUG(llvm::dbgs() << "\t\t" << i << ": " << *value << '\n');
      }
    }
  #endif

  return changed;
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitInst(Inst &i)
{
  i.dump();
  for (auto op : i.operand_values()) {
    if (auto inst = ::cast_or_null<Inst>(op)) {
      llvm::errs() << "\t" << ctx_.Find(inst) << "\n";
    }
  }
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitBarrierInst(BarrierInst &i)
{
  i.dump();
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitMemoryLoadInst(MemoryLoadInst &i)
{
  i.dump();
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitMemoryStoreInst(MemoryStoreInst &i)
{
  auto valueRef = i.GetValue();
  auto valueType = valueRef.GetType();
  auto value = ctx_.Find(valueRef);
  auto addr = ctx_.Find(i.GetAddr());

  switch (addr.GetKind()) {
    case SymbolicValue::Kind::UNKNOWN: {
      llvm_unreachable("not implemented");
    }
    case SymbolicValue::Kind::UNKNOWN_INTEGER: {
      llvm_unreachable("not implemented");
    }
    case SymbolicValue::Kind::INTEGER: {
      llvm_unreachable("not implemented");
    }
    case SymbolicValue::Kind::POINTER: {
      return heap_.Store(addr.GetPointer(), value, valueType);
    }
  }
  llvm_unreachable("invalid address kind");
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitMemoryExchangeInst(MemoryExchangeInst &i)
{
  i.dump();
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitMemoryCompareExchangeInst(MemoryCompareExchangeInst &i)
{
  i.dump();
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitLoadLinkInst(LoadLinkInst &i)
{
  i.dump();
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitStoreCondInst(StoreCondInst &i)
{
  i.dump();
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitArgInst(ArgInst &i)
{
  llvm::errs() << "\tTODO\n";
  return false;
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitMovInst(MovInst &i)
{
  auto arg = i.GetArg();
  switch (arg->GetKind()) {
    case Value::Kind::INST: {
      i.dump();
      llvm_unreachable("not implemented");
    }
    case Value::Kind::GLOBAL: {
      if (IsPointerType(i.GetType())) {
        return VisitMovGlobal(i, *::cast<Global>(arg), 0ll);
      }
      llvm_unreachable("invalid global type");
    }
    case Value::Kind::EXPR: {
      auto &sym = *::cast<SymbolOffsetExpr>(arg);
      if (IsPointerType(i.GetType())) {
        return VisitMovGlobal(i, *sym.GetSymbol(), sym.GetOffset());
      }
      llvm_unreachable("invalid expression type");
    }
    case Value::Kind::CONST: {
      auto &c = *::cast<Constant>(arg);
      switch (c.GetKind()) {
        case Constant::Kind::INT: {
          switch (auto ty = i.GetType()) {
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
                return ctx_.Set(i, SymbolicValue::Integer(value.trunc(width)));
              } else {
                return ctx_.Set(i, SymbolicValue::Integer(value));
              }
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

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitMovGlobal(Inst &i, Global &g, int64_t offset)
{
  switch (g.GetKind()) {
    case Global::Kind::FUNC: {
      llvm_unreachable("not implemented");
    }
    case Global::Kind::BLOCK: {
      llvm_unreachable("not implemented");
    }
    case Global::Kind::ATOM: {
      return ctx_.Set(i, SymbolicValue::Address(&g, offset));
    }
    case Global::Kind::EXTERN: {
      llvm_unreachable("not implemented");
    }
  }
  llvm_unreachable("invalid global kind");
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitSllInst(SllInst &i)
{
  class Visitor final : public BinaryVisitor<SllInst> {
  public:
    Visitor(SymbolicContext &ctx, const SllInst &i) : BinaryVisitor(ctx, i) {}

    SymbolicValue Visit(const APInt &lhs, const APInt &rhs) override
    {
      llvm_unreachable("not implemented");
    }
  };
  return ctx_.Set(i, Visitor(ctx_, i).Dispatch());
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitAddInst(AddInst &i)
{
  class Visitor final : public BinaryVisitor<AddInst> {
  public:
    Visitor(SymbolicContext &ctx, const AddInst &i) : BinaryVisitor(ctx, i) {}

    SymbolicValue Visit(UnknownInteger, const APInt &) override
    {
      return SymbolicValue::UnknownInteger();
    }

    SymbolicValue Visit(const SymbolicPointer &l, const APInt &r) override
    {
      if (r.getBitWidth() <= 64) {
        return SymbolicValue::Pointer(l.Offset(r.getSExtValue()));
      } else {
        llvm_unreachable("not implemented");
      }
    }

    SymbolicValue Visit(const APInt &l, const APInt &r) override
    {
      return SymbolicValue::Integer(l + r);
    }
  };
  return ctx_.Set(i, Visitor(ctx_, i).Dispatch());
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitCmpInst(CmpInst &i)
{
  class Visitor final : public BinaryVisitor<CmpInst> {
  public:
    Visitor(SymbolicContext &ctx, const CmpInst &i) : BinaryVisitor(ctx, i) {}

    SymbolicValue Visit(const APInt &l, const APInt &r) override
    {
      switch (inst_.GetCC()) {
        case Cond::EQ: case Cond::OEQ: case Cond::UEQ: return Flag(l == r);
        case Cond::NE: case Cond::ONE: case Cond::UNE: return Flag(l != r);
        case Cond::LT: case Cond::OLT: return Flag(l.slt(r));
        case Cond::ULT:                return Flag(l.ult(r));
        case Cond::GT: case Cond::OGT: return Flag(l.sgt(r));
        case Cond::UGT:                return Flag(l.ugt(r));
        case Cond::LE: case Cond::OLE: return Flag(l.sle(r));
        case Cond::ULE:                return Flag(l.ule(r));
        case Cond::GE: case Cond::OGE: return Flag(l.sge(r));
        case Cond::UGE:                return Flag(l.uge(r));
        case Cond::O:
        case Cond::UO: llvm_unreachable("invalid integer code");
      }
    }

    SymbolicValue Flag(bool value)
    {
      switch (auto ty = inst_.GetType()) {
        case Type::I8:
        case Type::I16:
        case Type::I32:
        case Type::I64:
        case Type::I128: {
          return SymbolicValue::Integer(APInt(GetSize(ty) * 8, value, true));
        }
        case Type::F32:
        case Type::F64:
        case Type::F80:
        case Type::V64:
        case Type::F128: {
          llvm_unreachable("invalid comparison");
        }
      }
      llvm_unreachable("invalid type");
    }
  };
  return ctx_.Set(i, Visitor(ctx_, i).Dispatch());
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitAndInst(AndInst &i)
{
  class Visitor final : public BinaryVisitor<AndInst> {
  public:
    Visitor(SymbolicContext &ctx, const AndInst &i) : BinaryVisitor(ctx, i) {}

    SymbolicValue Visit(const APInt &lhs, const APInt &rhs) override
    {
      llvm_unreachable("not implemented");
    }
  };
  return ctx_.Set(i, Visitor(ctx_, i).Dispatch());
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitX86_WrMsrInst(X86_WrMsrInst &i)
{
  llvm::errs() << "\tTODO\n";
  return false;
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitX86_RdTscInst(X86_RdTscInst &i)
{
  return ctx_.Set(i, SymbolicValue::UnknownInteger());
}
