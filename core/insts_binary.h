// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "inst.h"



/**
 * AddInst
 */
class AddInst final : public BinaryInst {
public:
  AddInst(Type type, Inst *lhs, Inst *rhs)
    : BinaryInst(Kind::ADD, type, lhs, rhs)
  {
  }
};

/**
 * AndInst
 */
class AndInst final : public BinaryInst {
public:
  AndInst(Type type, Inst *lhs, Inst *rhs)
    : BinaryInst(Kind::AND, type, lhs, rhs)
  {
  }
};

/**
 * CmpInst
 */
class CmpInst final : public BinaryInst {
public:
  /// Kind of the instruction.
  static constexpr Inst::Kind kInstKind = Inst::Kind::CMP;

public:
  CmpInst(Type type, Cond cc, Inst *lhs, Inst *rhs)
    : BinaryInst(Kind::CMP, type, lhs, rhs)
    , cc_(cc)
  {
  }

  /// Returns the condition code.
  Cond GetCC() const { return cc_; }

private:
  /// Condition code.
  Cond cc_;
};

/**
 * DivInst
 */
class DivInst final : public BinaryInst {
public:
  DivInst(Type type, Inst *lhs, Inst *rhs)
    : BinaryInst(Kind::DIV, type, lhs, rhs)
  {
  }
};

/**
 * MulInst
 */
class MulInst final : public BinaryInst {
public:
  MulInst(Type type, Inst *lhs, Inst *rhs)
    : BinaryInst(Kind::MUL, type, lhs, rhs)
  {
  }
};

/**
 * OrInst
 */
class OrInst final : public BinaryInst {
public:
  OrInst(Type type, Inst *lhs, Inst *rhs)
    : BinaryInst(Kind::OR, type, lhs, rhs)
  {
  }
};

/**
 * RemInst
 */
class RemInst final : public BinaryInst {
public:
  RemInst(Type type, Inst *lhs, Inst *rhs)
    : BinaryInst(Kind::REM, type, lhs, rhs)
  {
  }
};

/**
 * RotlInst
 */
class RotlInst final : public BinaryInst {
public:
  RotlInst(Type type, Inst *lhs, Inst *rhs)
    : BinaryInst(Kind::ROTL, type, lhs, rhs)
  {
  }
};

/**

 * SllInst
 */

class SllInst final : public BinaryInst {
public:
  SllInst(Type type, Inst *lhs, Inst *rhs)
    : BinaryInst(Kind::SLL, type, lhs, rhs)
  {
  }
};

/**
 * SraInst
 */
class SraInst final : public BinaryInst {
public:
  SraInst(Type type, Inst *lhs, Inst *rhs)
    : BinaryInst(Kind::SRA, type, lhs, rhs)
  {
  }
};

/**
 * SrlInst
 */
class SrlInst final : public BinaryInst {
public:
  SrlInst(Type type, Inst *lhs, Inst *rhs)
    : BinaryInst(Kind::SRL, type, lhs, rhs)
  {
  }
};

/**
 * SubInst
 */
class SubInst final : public BinaryInst {
public:
  SubInst(Type type, Inst *lhs, Inst *rhs)
    : BinaryInst(Kind::SUB, type, lhs, rhs)
  {
  }
};

/**
 * XorInst
 */
class XorInst final : public BinaryInst {
public:
  XorInst(Type type, Inst *lhs, Inst *rhs)
    : BinaryInst(Kind::XOR, type, lhs, rhs)
  {
  }
};

/**
 * PowInst
 */
class PowInst final : public BinaryInst {
public:
  PowInst(Type type, Inst *lhs, Inst *rhs)
    : BinaryInst(Kind::POW, type, lhs, rhs)
  {
  }
};

/**
 * CopySignInst
 */
class CopySignInst final : public BinaryInst {
public:
  CopySignInst(Type type, Inst *lhs, Inst *rhs)
    : BinaryInst(Kind::COPYSIGN, type, lhs, rhs)
  {
  }
};

/**
 * Overflow unsigned add inst.
 */
class AddUOInst final : public OverflowInst {
public:
  AddUOInst(Inst *lhs, Inst *rhs)
    : OverflowInst(Kind::UADDO, lhs, rhs)
  {
  }
};

/**
 * Overflow unsigned multiply inst.
 */
class MulUOInst final : public OverflowInst {
public:
  MulUOInst(Inst *lhs, Inst *rhs)
    : OverflowInst(Kind::UMULO, lhs, rhs)
  {
  }
};
