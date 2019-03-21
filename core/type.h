// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once



/**
 * Data Types known to the IR.
 */
enum class Type {
  I8, I16, I32, I64, I128,
  U8, U16, U32, U64, U128,
  F32, F64
};



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
