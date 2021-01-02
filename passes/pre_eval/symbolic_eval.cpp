// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/Support/Debug.h>
#include <llvm/Support/Format.h>

#include "core/expr.h"

#include "passes/pre_eval/symbolic_approx.h"
#include "passes/pre_eval/symbolic_context.h"
#include "passes/pre_eval/symbolic_eval.h"
#include "passes/pre_eval/symbolic_value.h"
#include "passes/pre_eval/symbolic_visitor.h"

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
bool SymbolicEval::Evaluate(Block &block)
{
  LLVM_DEBUG(llvm::dbgs() << "=======================================\n");
  LLVM_DEBUG(llvm::dbgs() << "Evaluating " << block.getName() << "\n");
  LLVM_DEBUG(llvm::dbgs() << "=======================================\n");

  bool changed = false;
  for (Inst &inst : block) {
    changed = Evaluate(inst) || changed;
  }
  return changed;
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitInst(Inst &i)
{
  llvm::errs() << "\n\nFAIL " << i << "\n";
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
  auto addr = ctx_.Find(i.GetAddr());
  switch (addr.GetKind()) {
    case SymbolicValue::Kind::UNKNOWN_INTEGER:
    case SymbolicValue::Kind::LOWER_BOUNDED_INTEGER:
    case SymbolicValue::Kind::INTEGER: {
      llvm_unreachable("not implemented");
    }
    case SymbolicValue::Kind::VALUE:
    case SymbolicValue::Kind::POINTER: {
      return ctx_.Set(i, ctx_.Load(addr.GetPointer(), i.GetType()));
    }
    case SymbolicValue::Kind::UNDEFINED: {
      llvm_unreachable("not implemented");
    }
  }
  llvm_unreachable("invalid address kind");
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitMemoryStoreInst(MemoryStoreInst &i)
{
  auto valueRef = i.GetValue();
  auto valueType = valueRef.GetType();
  auto value = ctx_.Find(valueRef);
  auto addr = ctx_.Find(i.GetAddr());

  switch (addr.GetKind()) {
    case SymbolicValue::Kind::UNKNOWN_INTEGER:
    case SymbolicValue::Kind::LOWER_BOUNDED_INTEGER:
    case SymbolicValue::Kind::INTEGER: {
      llvm_unreachable("not implemented");
    }
    case SymbolicValue::Kind::VALUE:
    case SymbolicValue::Kind::POINTER: {
      return ctx_.Store(addr.GetPointer(), value, valueType);
    }
    case SymbolicValue::Kind::UNDEFINED: {
      llvm_unreachable("not implemented");
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
bool SymbolicEval::VisitCallSite(CallSite &call)
{
  return SymbolicApprox(refs_, ctx_).Approximate(call);
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitPhiInst(PhiInst &phi)
{
  std::optional<SymbolicValue> values;
  for (unsigned i = 0, n = phi.GetNumIncoming(); i < n; ++i) {
    if (auto v = ctx_.FindOpt(phi.GetValue(i))) {
      if (values) {
        values = values->LUB(*v);
      } else {
        values = *v;
      }
    }
  }
  assert(values && "missing incoming values");
  return ctx_.Set(phi, *values);
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitArgInst(ArgInst &i)
{
  return ctx_.Set(i, ctx_.Arg(i.GetIndex()));
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitMovInst(MovInst &i)
{
  auto arg = i.GetArg();
  switch (arg->GetKind()) {
    case Value::Kind::INST: {
      return ctx_.Set(i, ctx_.Find(::cast<Inst>(arg)));
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
bool SymbolicEval::VisitTruncInst(TruncInst &i)
{
  auto arg = ctx_.Find(i.GetArg());
  switch (arg.GetKind()) {
    case SymbolicValue::Kind::UNKNOWN_INTEGER: {
      return ctx_.Set(i, arg);
    }
    case SymbolicValue::Kind::LOWER_BOUNDED_INTEGER: {
      llvm_unreachable("not implemented");
    }
    case SymbolicValue::Kind::INTEGER: {
      llvm_unreachable("not implemented");
    }
    case SymbolicValue::Kind::POINTER: {
      return ctx_.Set(i, SymbolicValue::UnknownInteger());
    }
    case SymbolicValue::Kind::UNDEFINED: {
      llvm_unreachable("not implemented");
    }
    case SymbolicValue::Kind::VALUE: {
      llvm_unreachable("not implemented");
    }
  }
  llvm_unreachable("invalid value kind");
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitZExtInst(ZExtInst &i)
{
  auto arg = ctx_.Find(i.GetArg());
  switch (arg.GetKind()) {
    case SymbolicValue::Kind::UNKNOWN_INTEGER:
    case SymbolicValue::Kind::UNDEFINED:
    case SymbolicValue::Kind::LOWER_BOUNDED_INTEGER: {
      return ctx_.Set(i, arg);
    }
    case SymbolicValue::Kind::INTEGER: {
      return ctx_.Set(i, SymbolicValue::Integer(
          arg.GetInteger().zext(GetBitWidth(i.GetType()))
      ));
    }
    case SymbolicValue::Kind::POINTER: {
      llvm_unreachable("not implemented");
    }
    case SymbolicValue::Kind::VALUE: {
      llvm_unreachable("not implemented");
    }
  }
  llvm_unreachable("invalid value kind");
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitSExtInst(SExtInst &i)
{
  auto arg = ctx_.Find(i.GetArg());
  switch (arg.GetKind()) {
    case SymbolicValue::Kind::UNKNOWN_INTEGER:
    case SymbolicValue::Kind::UNDEFINED:
    case SymbolicValue::Kind::LOWER_BOUNDED_INTEGER: {
      return ctx_.Set(i, arg);
    }
    case SymbolicValue::Kind::INTEGER: {
      return ctx_.Set(i, SymbolicValue::Integer(
          arg.GetInteger().sext(GetBitWidth(i.GetType()))
      ));
    }
    case SymbolicValue::Kind::POINTER: {
      llvm_unreachable("not implemented");
    }
    case SymbolicValue::Kind::VALUE: {
      llvm_unreachable("not implemented");
    }
  }
  llvm_unreachable("invalid value kind");
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitMovGlobal(Inst &i, Global &g, int64_t offset)
{
  switch (g.GetKind()) {
    case Global::Kind::FUNC: {
      return ctx_.Set(i, SymbolicValue::Pointer(static_cast<Func *>(&g)));
    }
    case Global::Kind::BLOCK: {
      llvm_unreachable("not implemented");
    }
    case Global::Kind::EXTERN:
    case Global::Kind::ATOM: {
      return ctx_.Set(i, SymbolicValue::Pointer(&g, offset));
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

    SymbolicValue Visit(const APInt &l, const APInt &r) override
    {
      return SymbolicValue::Integer(l.shl(r.getSExtValue()));
    }

    SymbolicValue Visit(const APInt &, LowerBoundedInteger) override
    {
      return SymbolicValue::UnknownInteger();
    }

    SymbolicValue Visit(LowerBoundedInteger l, const APInt &r) override
    {
      auto newBound = l.Bound.shl(r.getSExtValue());
      if (newBound.isNonNegative()) {
        return SymbolicValue::LowerBoundedInteger(newBound);
      } else {
        return SymbolicValue::UnknownInteger();
      }
    }

    SymbolicValue Visit(Pointer l, const APInt &r) override
    {
      return SymbolicValue::Pointer(l.Ptr.Decay());
    }
  };
  return ctx_.Set(i, Visitor(ctx_, i).Dispatch());
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitSrlInst(SrlInst &i)
{
  class Visitor final : public BinaryVisitor<SrlInst> {
  public:
    Visitor(SymbolicContext &ctx, const SrlInst &i) : BinaryVisitor(ctx, i) {}

    SymbolicValue Visit(Pointer l, const APInt &r) override
    {
      return SymbolicValue::Pointer(l.Ptr.Decay());
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

    SymbolicValue Visit(Pointer l, const APInt &r) override
    {
      if (r.getBitWidth() <= 64) {
        return SymbolicValue::Pointer(l.Ptr.Offset(r.getSExtValue()));
      } else {
        llvm_unreachable("not implemented");
      }
    }

    SymbolicValue Visit(Value l, const APInt &r) override
    {
      if (r.getBitWidth() <= 64) {
        return SymbolicValue::Value(l.Ptr.Offset(r.getSExtValue()));
      } else {
        llvm_unreachable("not implemented");
      }
    }

    SymbolicValue Visit(const APInt &l, const APInt &r) override
    {
      return SymbolicValue::Integer(l + r);
    }

    SymbolicValue Visit(LowerBoundedInteger l, const APInt &r) override
    {
      assert(l.Bound.getBitWidth() == r.getBitWidth() && "invalid operands");
      if (l.Bound.getBitWidth() <= 64) {
        auto newBound = l.Bound + r;
        if (newBound.isNonNegative()) {
          return SymbolicValue::LowerBoundedInteger(newBound);
        } else {
          return SymbolicValue::UnknownInteger();
        }
      } else {
          return SymbolicValue::UnknownInteger();
      }
    }

    SymbolicValue Visit(const APInt &l, LowerBoundedInteger r) override
    {
      return Visit(r, l);
    }

    SymbolicValue Visit(LowerBoundedInteger l, LowerBoundedInteger r) override
    {
      auto newBound = l.Bound + r.Bound;
      if (newBound.isNonNegative()) {
        return SymbolicValue::LowerBoundedInteger(newBound);
      } else {
        return SymbolicValue::UnknownInteger();
      }
    }

    SymbolicValue Visit(Value l, UnknownInteger) override
    {
      return SymbolicValue::Pointer(l.Ptr.Decay());
    }

    SymbolicValue Visit(Value l, LowerBoundedInteger) override
    {
      return SymbolicValue::Pointer(l.Ptr.Decay());
    }
  };
  return ctx_.Set(i, Visitor(ctx_, i).Dispatch());
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitSubInst(SubInst &i)
{
  class Visitor final : public BinaryVisitor<SubInst> {
  public:
    Visitor(SymbolicContext &ctx, const SubInst &i) : BinaryVisitor(ctx, i) {}

    SymbolicValue Visit(UnknownInteger, const APInt &) override
    {
      return SymbolicValue::UnknownInteger();
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

    SymbolicValue Visit(const APInt &l, LowerBoundedInteger r) override
    {
      if (l.isNonNegative()) {
        switch (inst_.GetCC()) {
          case Cond::GE: {
            if (l.ult(r.Bound)) {
              return Flag(false);
            }
            break;
          }
          case Cond::LT: {
            if (l.ult(r.Bound)) {
              return Flag(true);
            }
            break;
          }
          default: {
            llvm_unreachable("not implemented");
          }
        }
      }
      return SymbolicValue::UnknownInteger();
    }

    SymbolicValue Visit(LowerBoundedInteger l, const APInt &r) override
    {
      if (r.isNonNegative()) {
        switch (inst_.GetCC()) {
          case Cond::EQ: {
            if (r.ult(l.Bound)) {
              return Flag(false);
            }
            break;
          }
          case Cond::NE: {
            if (r.ult(l.Bound)) {
              return Flag(true);
            }
            break;
          }
          case Cond::LT: {
            if (l.Bound.ugt(r)) {
              return Flag(false);
            }
            break;
          }
          default: {
            llvm_unreachable("not implemented");
          }
        }
      }
      return SymbolicValue::UnknownInteger();
    }

    SymbolicValue Visit(Pointer l, const APInt &r) override
    {
      if (r.isNullValue()) {
        llvm_unreachable("not implemented");
      } else {
        return SymbolicValue::UnknownInteger();
      }
    }

    SymbolicValue Visit(Value l, const APInt &r) override
    {
      return SymbolicValue::UnknownInteger();
    }

    SymbolicValue Visit(LowerBoundedInteger l, LowerBoundedInteger r) override
    {
      return SymbolicValue::UnknownInteger();
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
bool SymbolicEval::VisitSelectInst(SelectInst &i)
{
  auto condVal = ctx_.Find(i.GetCond());
  auto trueVal = ctx_.Find(i.GetTrue());
  auto falseVal = ctx_.Find(i.GetFalse());
  if (condVal.IsTrue()) {
    return ctx_.Set(i, trueVal);
  }
  if (condVal.IsFalse()) {
    return ctx_.Set(i, falseVal);
  }
  return ctx_.Set(i, trueVal.LUB(falseVal));
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitFrameInst(FrameInst &i)
{
  return ctx_.Set(i, SymbolicValue::Pointer(
      ctx_.CurrentFrame(),
      i.GetObject(),
      i.GetOffset()
  ));
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitAndInst(AndInst &i)
{
  class Visitor final : public BinaryVisitor<AndInst> {
  public:
    Visitor(SymbolicContext &ctx, const AndInst &i) : BinaryVisitor(ctx, i) {}

    SymbolicValue Visit(const APInt &lhs, const APInt &rhs) override
    {
      return SymbolicValue::Integer(lhs & rhs);
    }

    SymbolicValue Visit(Pointer l, const APInt &rhs) override
    {
      if (rhs.getSExtValue() < (1 << 16)) {
        return SymbolicValue::UnknownInteger();
      }
      return SymbolicValue::Pointer(l.Ptr.Decay());
    }
  };
  return ctx_.Set(i, Visitor(ctx_, i).Dispatch());
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitOrInst(OrInst &i)
{
  class Visitor final : public BinaryVisitor<OrInst> {
  public:
    Visitor(SymbolicContext &ctx, const OrInst &i) : BinaryVisitor(ctx, i) {}

    SymbolicValue Visit(Pointer l, const APInt &rhs) override
    {
      if (rhs.isNullValue()) {
        return SymbolicValue::Pointer(l.Ptr);
      }
      return SymbolicValue::Pointer(l.Ptr.Decay());
    }

    SymbolicValue Visit(Pointer l, UnknownInteger) override
    {
      return SymbolicValue::Pointer(l.Ptr.Decay());
    }
  };
  return ctx_.Set(i, Visitor(ctx_, i).Dispatch());
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitUDivInst(UDivInst &i)
{
  class Visitor final : public BinaryVisitor<UDivInst> {
  public:
    Visitor(SymbolicContext &ctx, const UDivInst &i) : BinaryVisitor(ctx, i) {}

    SymbolicValue Visit(LowerBoundedInteger l, UnknownInteger) override
    {
      return SymbolicValue::UnknownInteger();
    }
  };
  return ctx_.Set(i, Visitor(ctx_, i).Dispatch());
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitMulInst(MulInst &i)
{
  class Visitor final : public BinaryVisitor<MulInst> {
  public:
    Visitor(SymbolicContext &ctx, const MulInst &i) : BinaryVisitor(ctx, i) {}
  };
  return ctx_.Set(i, Visitor(ctx_, i).Dispatch());
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitX86_OutInst(X86_OutInst &i)
{
  llvm::errs() << "\tTODO " << i << "\n";
  return false;
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitX86_LidtInst(X86_LidtInst &i)
{
  llvm::errs() << "\tTODO " << i << "\n";
  return false;
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitX86_LgdtInst(X86_LgdtInst &i)
{
  llvm::errs() << "\tTODO " << i << "\n";
  return false;
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitX86_LtrInst(X86_LtrInst &i)
{
  llvm::errs() << "\tTODO " << i << "\n";
  return false;
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitX86_SetCsInst(X86_SetCsInst &i)
{
  llvm::errs() << "\tTODO " << i << "\n";
  return false;
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitX86_SetDsInst(X86_SetDsInst &i)
{
  llvm::errs() << "\tTODO " << i << "\n";
  return false;
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitX86_WrMsrInst(X86_WrMsrInst &i)
{
  llvm::errs() << "\tTODO " << i << "\n";
  return false;
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitX86_RdTscInst(X86_RdTscInst &i)
{
  return ctx_.Set(i, SymbolicValue::UnknownInteger());
}
