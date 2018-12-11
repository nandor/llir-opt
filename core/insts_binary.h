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
  AddInst(Block *block, Type type, Inst *lhs, Inst *rhs)
    : BinaryInst(Kind::ADD, block, type, lhs, rhs)
  {
  }
};

/**
 * AndInst
 */
class AndInst final : public BinaryInst {
public:
  AndInst(Block *block, Type type, Inst *lhs, Inst *rhs)
    : BinaryInst(Kind::AND, block, type, lhs, rhs)
  {
  }
};

/**
 * CmpInst
 */
class CmpInst final : public BinaryInst {
public:
  CmpInst(Block *block, Type type, Cond cc, Inst *lhs, Inst *rhs)
    : BinaryInst(Kind::CMP, block, type, lhs, rhs)
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
  DivInst(Block *block, Type type, Inst *lhs, Inst *rhs)
    : BinaryInst(Kind::DIV, block, type, lhs, rhs)
  {
  }
};

/**
 * MulInst
 */
class MulInst final : public BinaryInst {
public:
  MulInst(Block *block, Type type, Inst *lhs, Inst *rhs)
    : BinaryInst(Kind::MUL, block, type, lhs, rhs)
  {
  }
};

/**
 * OrInst
 */
class OrInst final : public BinaryInst {
public:
  OrInst(Block *block, Type type, Inst *lhs, Inst *rhs)
    : BinaryInst(Kind::OR, block, type, lhs, rhs)
  {
  }
};

/**
 * RemInst
 */
class RemInst final : public BinaryInst {
public:
  RemInst(Block *block, Type type, Inst *lhs, Inst *rhs)
    : BinaryInst(Kind::REM, block, type, lhs, rhs)
  {
  }
};

/**
 * RotlInst
 */
class RotlInst final : public BinaryInst {
public:
  RotlInst(Block *block, Type type, Inst *lhs, Inst *rhs)
    : BinaryInst(Kind::ROTL, block, type, lhs, rhs)
  {
  }
};

/**

 * SllInst
 */

class SllInst final : public BinaryInst {
public:
  SllInst(Block *block, Type type, Inst *lhs, Inst *rhs)
    : BinaryInst(Kind::SLL, block, type, lhs, rhs)
  {
  }
};

/**
 * SraInst
 */
class SraInst final : public BinaryInst {
public:
  SraInst(Block *block, Type type, Inst *lhs, Inst *rhs)
    : BinaryInst(Kind::SRA, block, type, lhs, rhs)
  {
  }
};

/**
 * SrlInst
 */
class SrlInst final : public BinaryInst {
public:
  SrlInst(Block *block, Type type, Inst *lhs, Inst *rhs)
    : BinaryInst(Kind::SRL, block, type, lhs, rhs)
  {
  }
};

/**
 * SubInst
 */
class SubInst final : public BinaryInst {
public:
  SubInst(Block *block, Type type, Inst *lhs, Inst *rhs)
    : BinaryInst(Kind::SUB, block, type, lhs, rhs)
  {
  }
};

/**
 * XorInst
 */
class XorInst final : public BinaryInst {
public:
  XorInst(Block *block, Type type, Inst *lhs, Inst *rhs)
    : BinaryInst(Kind::XOR, block, type, lhs, rhs)
  {
  }
};

/**
 * PowInst
 */
class PowInst final : public BinaryInst {
public:
  PowInst(Block *block, Type type, Inst *lhs, Inst *rhs)
    : BinaryInst(Kind::POW, block, type, lhs, rhs)
  {
  }
};

/**
 * CopySignInst
 */
class CopySignInst final : public BinaryInst {
public:
  CopySignInst(Block *block, Type type, Inst *lhs, Inst *rhs)
    : BinaryInst(Kind::POW, block, type, lhs, rhs)
  {
  }
};

/**
 * Overflow unsigned add inst.
 */
class AddUOInst final : public OverflowInst {
public:
  AddUOInst(Block *block, Inst *lhs, Inst *rhs)
    : OverflowInst(Kind::UADDO, block, lhs, rhs)
  {
  }
};

/**
 * Overflow unsigned multiply inst.
 */
class MulUOInst final : public OverflowInst {
public:
  MulUOInst(Block *block, Inst *lhs, Inst *rhs)
    : OverflowInst(Kind::UMULO, block, lhs, rhs)
  {
  }
};
