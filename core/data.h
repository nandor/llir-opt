// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <cstdint>



/**
 * Class representing a value in the data section.
 */
class Value {
public:
};

/**
 * Class representing an integer value.
 */
class IntValue : public Value {
public:
};

/**
 * Class representing a symbol value.
 */
class SymValue : public Value {
public:
};

/**
 * Class representing an expression value.
 */
class ExprValue : public Value {
public:
};


/**
 * The data segment of a program.
 */
class Data {
public:
  void Align(unsigned i);
  void AddInt8(Value *v);
  void AddInt16(Value *v);
  void AddInt32(Value *v);
  void AddInt64(Value *v);
  void AddFloat64(Value *v);
  void AddZero(Value *v);

private:
};
