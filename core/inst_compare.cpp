// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/inst_compare.h"



// -----------------------------------------------------------------------------
bool InstCompare::Equal(ConstRef<Inst> a, ConstRef<Inst> b) const
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
bool InstCompare::Equal(ConstRef<Global> a, ConstRef<Global> b) const
{
  return a.Get() == b.Get();
}

// -----------------------------------------------------------------------------
bool InstCompare::Equal(ConstRef<Expr> a, ConstRef<Expr> b) const
{
  if (a->GetKind() != b->GetKind()) {
    return false;
  }
  switch (a->GetKind()) {
    case Expr::Kind::SYMBOL_OFFSET: {
      auto as = ::cast<SymbolOffsetExpr>(a);
      auto bs = ::cast<SymbolOffsetExpr>(b);
      if (as->GetSymbol() != bs->GetSymbol()) {
        return false;
      }
      if (as->GetOffset() != bs->GetOffset()) {
        return false;
      }
      return true;
    }
  }
  llvm_unreachable("invalid symbol kind");
}

// -----------------------------------------------------------------------------
bool InstCompare::Equal(ConstRef<Constant> a, ConstRef<Constant> b) const
{
  if (a->GetKind() != b->GetKind()) {
    return false;
  }
  switch (a->GetKind()) {
    case Constant::Kind::INT: {
      auto v1 = cast<ConstantInt>(a);
      auto v2 = cast<ConstantInt>(b);
      const auto &int1 = v1->GetValue();
      const auto &int2 = v2->GetValue();
      return int1.getBitWidth() == int2.getBitWidth() && int1 == int2;
    }
    case Constant::Kind::FLOAT: {
      auto v1 = cast<ConstantFloat>(a);
      auto v2 = cast<ConstantFloat>(b);
      const auto &float1 = v1->GetValue();
      const auto &float2 = v2->GetValue();
      return float1.bitwiseIsEqual(float2) == llvm::APFloat::cmpEqual;
    }
  }
  llvm_unreachable("invalid constant kind");
}

// -----------------------------------------------------------------------------
bool InstCompare::Equal(ConstRef<Value> a, ConstRef<Value> b) const
{
  if (a->GetKind() != b->GetKind()) {
    return false;
  }
  if (a.Index() != b.Index()) {
    return false;
  }
  switch (a->GetKind()) {
    case Value::Kind::INST: {
      return Equal(cast<Inst>(a), cast<Inst>(b));
    }
    case Value::Kind::GLOBAL: {
      return Equal(cast<Global>(a), cast<Global>(b));
    }
    case Value::Kind::EXPR: {
      return Equal(cast<Expr>(a), cast<Expr>(b));
    }
    case Value::Kind::CONST: {
      return Equal(cast<Constant>(a), cast<Constant>(b));
    }
  }
  llvm_unreachable("invalid global kind");
}

// -----------------------------------------------------------------------------
bool InstCompare::Equal(const Block *a, const Block *b) const
{
  return a == b;
}

// -----------------------------------------------------------------------------
bool InstCompare::Equal(const Inst &a, const Inst &b) const
{
  if (a.GetAnnots() != b.GetAnnots()) {
    return false;
  }
  if (a.GetKind() != b.GetKind()) {
    return false;
  }
  switch (a.GetKind()) {
    case Inst::Kind::PHI: {
      auto &pa = static_cast<const PhiInst &>(a);
      auto &pb = static_cast<const PhiInst &>(b);
      unsigned n = pa.GetNumIncoming();
      if (n != pb.GetNumIncoming()) {
        return false;
      }
      for (unsigned i = 0; i < n; ++i) {
        if (!Equal(pa.GetBlock(i), pb.GetBlock(i))) {
          return false;
        }
        if (!Equal(pa.GetValue(i), pb.GetValue(i))) {
          return false;
        }
      }
      return true;
    }
    #define GET_COMPARE
    #include "instructions.def"
  }
  llvm_unreachable("invalid instruction kind");
}
