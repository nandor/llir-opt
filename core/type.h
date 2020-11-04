// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/MachineValueType.h>
#include <llvm/Support/Alignment.h>



/**
 * Data Types known to the IR.
 */
enum class Type {
  I8,
  I16,
  I32,
  I64,
  V64,
  I128,
  F32,
  F64,
  F80,
  F128
};

/**
 * Prints a type to a stream.
 */
llvm::raw_ostream &operator<<(llvm::raw_ostream &os, Type ty);

/**
 * Checks if the type is an integer type.
 */
bool IsIntegerType(Type type);

/**
 * Checks if the type is a pointer type.
 */
bool IsPointerType(Type type);

/**
 * Checks if the type is a floating point type.
 */
bool IsFloatType(Type type);

/**
 * Returns the size of a type in bytes.
 */
unsigned GetSize(Type type);

/**
 * Returns the alignment of the type in bytes.
 */
llvm::Align GetAlignment(Type type);

/**
 * Returns the equivalent LLVM MachineValueType.
 */
llvm::MVT GetVT(Type type);
