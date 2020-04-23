// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/type.h"


// -----------------------------------------------------------------------------
llvm::raw_ostream &operator<<(llvm::raw_ostream &os, Type ty)
{
  switch (ty) {
    case Type::I8:    os << "i8";   break;
    case Type::I16:   os << "i16";  break;
    case Type::I32:   os << "i32";  break;
    case Type::I64:   os << "i64";  break;
    case Type::I128:  os << "i128"; break;
    case Type::F32:   os << "f32";  break;
    case Type::F64:   os << "f64";  break;
    case Type::F80:   os << "f80";  break;
  }
  return os;
}

// -----------------------------------------------------------------------------
bool IsIntegerType(Type type)
{
  switch (type) {
    case Type::F32:
    case Type::F64:
    case Type::F80:
      return false;
    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::I64:
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
      return false;
    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::I128:
      return false;
    case Type::I64:
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
    case Type::I8:
      return 1;
    case Type::I16:
      return 2;
    case Type::I32:
      return 4;
    case Type::I64:
      return 8;
    case Type::I128:
      return 16;
  }
  llvm_unreachable("invalid type");
}
