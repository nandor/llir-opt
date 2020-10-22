// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/inst.h"



/**
 * AddInst
 */
class AddInst final : public BinaryInst {
public:
  AddInst(Type type, Ref<Inst> lhs, Ref<Inst> rhs, AnnotSet &&annot)
    : BinaryInst(Kind::ADD, type, lhs, rhs, std::move(annot))
  {
  }
};

/**
 * AndInst
 */
class AndInst final : public BinaryInst {
public:
  AndInst(Type type, Ref<Inst> lhs, Ref<Inst> rhs, AnnotSet &&annot)
    : BinaryInst(Kind::AND, type, lhs, rhs, std::move(annot))
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
  CmpInst(Type type, Cond cc, Ref<Inst> lhs, Ref<Inst> rhs, AnnotSet &&annot)
    : BinaryInst(Kind::CMP, type, lhs, rhs, std::move(annot))
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
 * UDivInst
 */
class UDivInst final : public BinaryInst {
public:
  UDivInst(Type type, Ref<Inst> lhs, Ref<Inst> rhs, AnnotSet &&annot)
    : BinaryInst(Kind::UDIV, type, lhs, rhs, std::move(annot))
  {
  }
};

/**
 * SDivInst
 */
class SDivInst final : public BinaryInst {
public:
  SDivInst(Type type, Ref<Inst> lhs, Ref<Inst> rhs, AnnotSet &&annot)
    : BinaryInst(Kind::SDIV, type, lhs, rhs, std::move(annot))
  {
  }
};

/**
 * URemInst
 */
class URemInst final : public BinaryInst {
public:
  URemInst(Type type, Ref<Inst> lhs, Ref<Inst> rhs, AnnotSet &&annot)
    : BinaryInst(Kind::UREM, type, lhs, rhs, std::move(annot))
  {
  }
};

/**
 * SRemInst
 */
class SRemInst final : public BinaryInst {
public:
  SRemInst(Type type, Ref<Inst> lhs, Ref<Inst> rhs, AnnotSet &&annot)
    : BinaryInst(Kind::SREM, type, lhs, rhs, std::move(annot))
  {
  }
};


/**
 * MulInst
 */
class MulInst final : public BinaryInst {
public:
  /// Kind of the instruction.
  static constexpr Inst::Kind kInstKind = Inst::Kind::MUL;

public:
  MulInst(Type type, Ref<Inst> lhs, Ref<Inst> rhs, AnnotSet &&annot)
    : BinaryInst(Kind::MUL, type, lhs, rhs, std::move(annot))
  {
  }
};

/**
 * OrInst
 */
class OrInst final : public BinaryInst {
public:
  OrInst(Type type, Ref<Inst> lhs, Ref<Inst> rhs, AnnotSet &&annot)
    : BinaryInst(Kind::OR, type, lhs, rhs, std::move(annot))
  {
  }
};

/**
 * RotlInst
 */
class RotlInst final : public BinaryInst {
public:
  RotlInst(Type type, Ref<Inst> lhs, Ref<Inst> rhs, AnnotSet &&annot)
    : BinaryInst(Kind::ROTL, type, lhs, rhs, std::move(annot))
  {
  }
};

/**
 * RotrInst
 */
class RotrInst final : public BinaryInst {
public:
  RotrInst(Type type, Ref<Inst> lhs, Ref<Inst> rhs, AnnotSet &&annot)
    : BinaryInst(Kind::ROTR, type, lhs, rhs, std::move(annot))
  {
  }
};


/**

 * SllInst
 */

class SllInst final : public BinaryInst {
public:
  SllInst(Type type, Ref<Inst> lhs, Ref<Inst> rhs, AnnotSet &&annot)
    : BinaryInst(Kind::SLL, type, lhs, rhs, std::move(annot))
  {
  }
};

/**
 * SraInst
 */
class SraInst final : public BinaryInst {
public:
  SraInst(Type type, Ref<Inst> lhs, Ref<Inst> rhs, AnnotSet &&annot)
    : BinaryInst(Kind::SRA, type, lhs, rhs, std::move(annot))
  {
  }
};

/**
 * SrlInst
 */
class SrlInst final : public BinaryInst {
public:
  SrlInst(Type type, Ref<Inst> lhs, Ref<Inst> rhs, AnnotSet &&annot)
    : BinaryInst(Kind::SRL, type, lhs, rhs, std::move(annot))
  {
  }
};

/**
 * SubInst
 */
class SubInst final : public BinaryInst {
public:
  SubInst(Type type, Ref<Inst> lhs, Ref<Inst> rhs, AnnotSet &&annot)
    : BinaryInst(Kind::SUB, type, lhs, rhs, std::move(annot))
  {
  }
};

/**
 * XorInst
 */
class XorInst final : public BinaryInst {
public:
  XorInst(Type type, Ref<Inst> lhs, Ref<Inst> rhs, AnnotSet &&annot)
    : BinaryInst(Kind::XOR, type, lhs, rhs, std::move(annot))
  {
  }
};

/**
 * PowInst
 */
class PowInst final : public BinaryInst {
public:
  PowInst(Type type, Ref<Inst> lhs, Ref<Inst> rhs, AnnotSet &&annot)
    : BinaryInst(Kind::POW, type, lhs, rhs, std::move(annot))
  {
  }
};

/**
 * CopySignInst
 */
class CopySignInst final : public BinaryInst {
public:
  CopySignInst(Type type, Ref<Inst> lhs, Ref<Inst> rhs, AnnotSet &&annot)
    : BinaryInst(Kind::COPYSIGN, type, lhs, rhs, std::move(annot))
  {
  }
};

/**
 * Overflow unsigned add inst.
 */
class AddUOInst final : public OverflowInst {
public:
  AddUOInst(Type type, Ref<Inst> lhs, Ref<Inst> rhs, AnnotSet &&annot)
    : OverflowInst(Kind::UADDO, type, lhs, rhs, std::move(annot))
  {
  }
};


/**
 * Overflow signed add inst.
 */
class AddSOInst final : public OverflowInst {
public:
  AddSOInst(Type type, Ref<Inst> lhs, Ref<Inst> rhs, AnnotSet &&annot)
    : OverflowInst(Kind::SADDO, type, lhs, rhs, std::move(annot))
  {
  }
};

/**
 * Overflow unsigned multiply inst.
 */
class MulUOInst final : public OverflowInst {
public:
  MulUOInst(Type type, Ref<Inst> lhs, Ref<Inst> rhs, AnnotSet &&annot)
    : OverflowInst(Kind::UMULO, type, lhs, rhs, std::move(annot))
  {
  }
};

/**
 * Overflow unsigned multiply inst.
 */
class MulSOInst final : public OverflowInst {
public:
  MulSOInst(Type type, Ref<Inst> lhs, Ref<Inst> rhs, AnnotSet &&annot)
    : OverflowInst(Kind::SMULO, type, lhs, rhs, std::move(annot))
  {
  }
};

/**
 * Overflow unsigned multiply inst.
 */
class SubUOInst final : public OverflowInst {
public:
  SubUOInst(Type type, Ref<Inst> lhs, Ref<Inst> rhs, AnnotSet &&annot)
    : OverflowInst(Kind::USUBO, type, lhs, rhs, std::move(annot))
  {
  }
};

/**
 * Overflow unsigned multiply inst.
 */
class SubSOInst final : public OverflowInst {
public:
  SubSOInst(Type type, Ref<Inst> lhs, Ref<Inst> rhs, AnnotSet &&annot)
    : OverflowInst(Kind::SSUBO, type, lhs, rhs, std::move(annot))
  {
  }
};
