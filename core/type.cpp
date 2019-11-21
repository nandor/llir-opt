// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/type.h"



// -----------------------------------------------------------------------------
bool IsIntegerType(Type type)
{
  switch (type) {
    case Type::F32:
    case Type::F64:
      return false;
    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::I64:
    case Type::I128:
      return true;
    case Type::U8:
    case Type::U16:
    case Type::U32:
    case Type::U64:
    case Type::U128:
      return true;
  }
}

// -----------------------------------------------------------------------------
bool IsSigned(Type type)
{
  switch (type) {
    case Type::F32:
    case Type::F64:
      return false;
    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::I64:
    case Type::I128:
      return true;
    case Type::U8:
    case Type::U16:
    case Type::U32:
    case Type::U64:
    case Type::U128:
      return false;
  }
}


// -----------------------------------------------------------------------------
bool IsUnsigned(Type type)
{
  switch (type) {
    case Type::F32:
    case Type::F64:
      return false;
    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::I64:
    case Type::I128:
      return false;
    case Type::U8:
    case Type::U16:
    case Type::U32:
    case Type::U64:
    case Type::U128:
      return true;
  }
}

// -----------------------------------------------------------------------------
bool IsPointerType(Type type)
{
  switch (type) {
    case Type::F32:
    case Type::F64:
      return false;
    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::I128:
      return false;
    case Type::U8:
    case Type::U16:
    case Type::U32:
    case Type::U128:
      return false;
    case Type::I64:
    case Type::U64:
      return true;
  }
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
    case Type::I8:
    case Type::U8:
      return 1;
    case Type::I16:
    case Type::U16:
      return 2;
    case Type::I32:
    case Type::U32:
      return 4;
    case Type::I64:
    case Type::U64:
      return 8;
    case Type::I128:
    case Type::U128:
      return 16;
  }
}
