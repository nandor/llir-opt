// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <cassert>
#include <cstdint>

class Expr;
class Inst;
class Symbol;
class Block;
class Value;



/**
 * Registers.
 */
enum class Reg {
  SP,
  FP,
};



/**
 * Operand to an instruction.
 */
class Operand {
public:
  enum class Kind {
    INT    = 0,
    FLOAT  = 1,
    REG    = 2,
    UNDEF  = 3,
    VALUE  = 4,
  };

  Operand(int64_t intData) : type_(Kind::INT), intData_(intData) { }
  Operand(double floatData) : type_(Kind::FLOAT), floatData_(floatData) { }
  Operand(Reg regData) : type_(Kind::REG), regData_(regData) {  }
  Operand(Value *valueData) : type_(Kind::VALUE), valueData_(valueData) { }
  Operand() : type_(Kind::UNDEF) { }

  Kind GetKind() const { return type_; }
  bool IsInt() const { return type_ == Kind::INT; }
  bool IsFloat() const { return type_ == Kind::FLOAT; }
  bool IsReg() const { return type_ == Kind::REG; }
  bool IsUndef() const { return type_ == Kind::UNDEF; }
  bool IsValue() const { return type_ == Kind::VALUE; }
  bool IsInst() const;
  bool IsSym() const;
  bool IsExpr() const;
  bool IsBlock() const;

  int64_t GetInt() const { assert(IsInt()); return intData_; }
  double GetFloat() const { assert(IsFloat()); return floatData_; }
  Reg GetReg() const { assert(IsReg()); return regData_; }
  Value *GetValue() const { assert(IsValue()); return valueData_; }
  Inst *GetInst() const;
  Symbol *GetSym() const;
  Expr *GetExpr() const;
  Block *GetBlock() const;

private:
  Kind type_;

  union {
    int64_t intData_;
    double floatData_;
    Reg regData_;
    Value *valueData_;
  };
};
