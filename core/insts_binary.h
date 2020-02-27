// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "inst.h"



/**
 * AddInst
 */
class AddInst final : public BinaryInst {
public:
  AddInst(Type type, Inst *lhs, Inst *rhs, const AnnotSet &annot)
    : BinaryInst(Kind::ADD, type, lhs, rhs, annot)
  {
  }
};

/**
 * AndInst
 */
class AndInst final : public BinaryInst {
public:
  AndInst(Type type, Inst *lhs, Inst *rhs, const AnnotSet &annot)
    : BinaryInst(Kind::AND, type, lhs, rhs, annot)
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
  CmpInst(Type type, Cond cc, Inst *lhs, Inst *rhs, const AnnotSet &annot)
    : BinaryInst(Kind::CMP, type, lhs, rhs, annot)
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
  DivInst(Type type, Inst *lhs, Inst *rhs, const AnnotSet &annot)
    : BinaryInst(Kind::DIV, type, lhs, rhs, annot)
  {
  }
};

/**
 * MulInst
 */
class MulInst final : public BinaryInst {
public:
  MulInst(Type type, Inst *lhs, Inst *rhs, const AnnotSet &annot)
    : BinaryInst(Kind::MUL, type, lhs, rhs, annot)
  {
  }
};

/**
 * OrInst
 */
class OrInst final : public BinaryInst {
public:
  OrInst(Type type, Inst *lhs, Inst *rhs, const AnnotSet &annot)
    : BinaryInst(Kind::OR, type, lhs, rhs, annot)
  {
  }
};

/**
 * RemInst
 */
class RemInst final : public BinaryInst {
public:
  RemInst(Type type, Inst *lhs, Inst *rhs, const AnnotSet &annot)
    : BinaryInst(Kind::REM, type, lhs, rhs, annot)
  {
  }
};

/**
 * RotlInst
 */
class RotlInst final : public BinaryInst {
public:
  RotlInst(Type type, Inst *lhs, Inst *rhs, const AnnotSet &annot)
    : BinaryInst(Kind::ROTL, type, lhs, rhs, annot)
  {
  }
};

/**

 * SllInst
 */

class SllInst final : public BinaryInst {
public:
  SllInst(Type type, Inst *lhs, Inst *rhs, const AnnotSet &annot)
    : BinaryInst(Kind::SLL, type, lhs, rhs, annot)
  {
  }
};

/**
 * SraInst
 */
class SraInst final : public BinaryInst {
public:
  SraInst(Type type, Inst *lhs, Inst *rhs, const AnnotSet &annot)
    : BinaryInst(Kind::SRA, type, lhs, rhs, annot)
  {
  }
};

/**
 * SrlInst
 */
class SrlInst final : public BinaryInst {
public:
  SrlInst(Type type, Inst *lhs, Inst *rhs, const AnnotSet &annot)
    : BinaryInst(Kind::SRL, type, lhs, rhs, annot)
  {
  }
};

/**
 * SubInst
 */
class SubInst final : public BinaryInst {
public:
  SubInst(Type type, Inst *lhs, Inst *rhs, const AnnotSet &annot)
    : BinaryInst(Kind::SUB, type, lhs, rhs, annot)
  {
  }
};

/**
 * XorInst
 */
class XorInst final : public BinaryInst {
public:
  XorInst(Type type, Inst *lhs, Inst *rhs, const AnnotSet &annot)
    : BinaryInst(Kind::XOR, type, lhs, rhs, annot)
  {
  }
};

/**
 * PowInst
 */
class PowInst final : public BinaryInst {
public:
  PowInst(Type type, Inst *lhs, Inst *rhs, const AnnotSet &annot)
    : BinaryInst(Kind::POW, type, lhs, rhs, annot)
  {
  }
};

/**
 * CopySignInst
 */
class CopySignInst final : public BinaryInst {
public:
  CopySignInst(Type type, Inst *lhs, Inst *rhs, const AnnotSet &annot)
    : BinaryInst(Kind::COPYSIGN, type, lhs, rhs, annot)
  {
  }
};

/**
 * Overflow unsigned add inst.
 */
class AddUOInst final : public OverflowInst {
public:
  AddUOInst(Type type, Inst *lhs, Inst *rhs, const AnnotSet &annot)
    : OverflowInst(Kind::UADDO, type, lhs, rhs, annot)
  {
  }
};

/**
 * Overflow unsigned multiply inst.
 */
class MulUOInst final : public OverflowInst {
public:
  MulUOInst(Type type, Inst *lhs, Inst *rhs, const AnnotSet &annot)
    : OverflowInst(Kind::UMULO, type, lhs, rhs, annot)
  {
  }
};
