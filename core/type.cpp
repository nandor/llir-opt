// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/type.h"


// -----------------------------------------------------------------------------
TypeFlag TypeFlag::GetNone()
{
  TypeFlag flag;
  flag.kind_ = static_cast<uint8_t>(Kind::NONE);
  return flag;
}

// -----------------------------------------------------------------------------
TypeFlag TypeFlag::GetSExt()
{
  TypeFlag flag;
  flag.kind_ = static_cast<uint8_t>(Kind::SEXT);
  return flag;
}

// -----------------------------------------------------------------------------
TypeFlag TypeFlag::GetZExt()
{
  TypeFlag flag;
  flag.kind_ = static_cast<uint8_t>(Kind::ZEXT);
  return flag;
}

// -----------------------------------------------------------------------------
TypeFlag TypeFlag::GetByVal(unsigned size, llvm::Align align)
{
  TypeFlag flag;
  flag.kind_ = static_cast<uint8_t>(Kind::BYVAL);
  flag.size_ = size;
  flag.align_ = align.value();
  return flag;
}

// -----------------------------------------------------------------------------
unsigned TypeFlag::GetByValSize() const
{
  assert(GetKind() == Kind::BYVAL && "not a byval flag");
  return size_;
}

// -----------------------------------------------------------------------------
llvm::Align TypeFlag::GetByValAlign() const
{
  assert(GetKind() == Kind::BYVAL && "not a byval flag");
  return llvm::Align(align_);
}


// -----------------------------------------------------------------------------
llvm::raw_ostream &operator<<(llvm::raw_ostream &os, Type type)
{
  switch (type) {
    case Type::I8:    return os << "i8";
    case Type::I16:   return os << "i16";
    case Type::I32:   return os << "i32";
    case Type::I64:   return os << "i64";
    case Type::V64:   return os << "v64";
    case Type::I128:  return os << "i128";
    case Type::F32:   return os << "f32";
    case Type::F64:   return os << "f64";
    case Type::F80:   return os << "f80";
    case Type::F128:  return os << "f128";
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
llvm::raw_ostream &operator<<(llvm::raw_ostream &os, TypeFlag flag)
{
  switch (flag.GetKind()) {
    case TypeFlag::Kind::NONE: {
      return os;
    }
    case TypeFlag::Kind::SEXT: {
      os << ":sext";
      return os;
    }
    case TypeFlag::Kind::ZEXT: {
      os << ":zext";
      return os;
    }
    case TypeFlag::Kind::BYVAL: {
      os << ":byval";
      os << ":" << flag.GetByValSize();
      os << ":" << flag.GetByValAlign().value();
      return os;
    }
  }
  llvm_unreachable("invalid type flag");
}

// -----------------------------------------------------------------------------
llvm::raw_ostream &operator<<(llvm::raw_ostream &os, FlaggedType type)
{
  return os << type.GetType() << type.GetFlag();
}

// -----------------------------------------------------------------------------
bool IsIntegerType(Type type)
{
  switch (type) {
    case Type::F32:
    case Type::F64:
    case Type::F80:
    case Type::F128:
      return false;
    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::I64:
    case Type::V64:
    case Type::I128:
      return true;
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
bool IsPointerType(Type type)
{
  switch (type) {
    case Type::F32:
    case Type::F64:
    case Type::F80:
    case Type::F128:
      return false;
    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::I128:
      return false;
    case Type::I64:
    case Type::V64:
      return true;
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
bool IsFloatType(Type type)
{
  return !IsIntegerType(type);
}

// -----------------------------------------------------------------------------
unsigned GetSize(Type type)
{
  switch (type) {
    case Type::F32:
      return 4;
    case Type::F64:
      return 8;
    case Type::F80:
      return 10;
    case Type::F128:
      return 16;
    case Type::I8:
      return 1;
    case Type::I16:
      return 2;
    case Type::I32:
      return 4;
    case Type::I64:
    case Type::V64:
      return 8;
    case Type::I128:
      return 16;
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
unsigned GetBitWidth(Type type)
{
  return GetSize(type) * 8;
}

// -----------------------------------------------------------------------------
llvm::Align GetAlignment(Type type)
{
  switch (type) {
    case Type::I8:
      return llvm::Align(1);
    case Type::I16:
      return llvm::Align(2);
    case Type::I32:
      return llvm::Align(4);
    case Type::I64:
    case Type::V64:
      return llvm::Align(8);
    case Type::I128:
      return llvm::Align(16);
    case Type::F32:
      return llvm::Align(4);
    case Type::F64:
      return llvm::Align(8);
    case Type::F80:
      return llvm::Align(1);
    case Type::F128:
      return llvm::Align(16);
  }
  llvm_unreachable("invalid type");
}

// -----------------------------------------------------------------------------
llvm::MVT GetVT(Type type)
{
  switch (type) {
    case Type::I8:   return llvm::MVT::i8;
    case Type::I16:  return llvm::MVT::i16;
    case Type::I32:  return llvm::MVT::i32;
    case Type::I64:  return llvm::MVT::i64;
    case Type::V64:  return llvm::MVT::i64;
    case Type::F32:  return llvm::MVT::f32;
    case Type::F64:  return llvm::MVT::f64;
    case Type::F80:  return llvm::MVT::f80;
    case Type::I128: return llvm::MVT::i128;
    case Type::F128: return llvm::MVT::f128;
  }
  llvm_unreachable("invalid type");
}
