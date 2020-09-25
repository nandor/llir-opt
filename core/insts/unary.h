// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/inst.h"



/**
 * AbsInst
 */
class AbsInst final : public UnaryInst {
public:
  AbsInst(Type type, Inst *op, AnnotSet &&annot)
    : UnaryInst(Kind::ABS, type, op, std::move(annot))
  {
  }
};

/**
 * NegInst
 */
class NegInst final : public UnaryInst {
public:
  NegInst(Type type, Inst *op, AnnotSet &&annot)
    : UnaryInst(Kind::NEG, type, op, std::move(annot))
  {
  }
};

/**
 * SqrtInst
 */
class SqrtInst final : public UnaryInst {
public:
  SqrtInst(Type type, Inst *op, AnnotSet &&annot)
    : UnaryInst(Kind::SQRT, type, op, std::move(annot))
  {
  }
};

/**
 * SinInst
 */
class SinInst final : public UnaryInst {
public:
  SinInst(Type type, Inst *op, AnnotSet &&annot)
    : UnaryInst(Kind::SIN, type, op, std::move(annot))
  {
  }
};

/**
 * CosInst
 */
class CosInst final : public UnaryInst {
public:
  CosInst(Type type, Inst *op, AnnotSet &&annot)
    : UnaryInst(Kind::COS, type, op, std::move(annot))
  {
  }
};

/**
 * SExtInst
 */
class SExtInst final : public UnaryInst {
public:
  SExtInst(Type type, Inst *op, AnnotSet &&annot)
    : UnaryInst(Kind::SEXT, type, op, std::move(annot))
  {
  }
};

/**
 * ZExtInst
 */
class ZExtInst final : public UnaryInst {
public:
  ZExtInst(Type type, Inst *op, AnnotSet &&annot)
    : UnaryInst(Kind::ZEXT, type, op, std::move(annot))
  {
  }
};

/**
 * XExtInst
 */
class XExtInst final : public UnaryInst {
public:
  XExtInst(Type type, Inst *op, AnnotSet &&annot)
    : UnaryInst(Kind::XEXT, type, op, std::move(annot))
  {
  }
};

/**
 * FExtInst
 */
class FExtInst final : public UnaryInst {
public:
  FExtInst(Type type, Inst *op, AnnotSet &&annot)
    : UnaryInst(Kind::FEXT, type, op, std::move(annot))
  {
  }
};

/**
 * TruncInst
 */
class TruncInst final : public UnaryInst {
public:
  TruncInst(Type type, Inst *op, AnnotSet &&annot)
    : UnaryInst(Kind::TRUNC, type, op, std::move(annot))
  {
  }
};

/**
 * ExpInst
 */
class ExpInst final : public UnaryInst {
public:
  ExpInst(Type type, Inst *op, AnnotSet &&annot)
    : UnaryInst(Kind::EXP, type, op, std::move(annot))
  {
  }
};

/**
 * Exp2Inst
 */
class Exp2Inst final : public UnaryInst {
public:
  Exp2Inst(Type type, Inst *op, AnnotSet &&annot)
    : UnaryInst(Kind::EXP2, type, op, std::move(annot))
  {
  }
};

/**
 * LogInst
 */
class LogInst final : public UnaryInst {
public:
  LogInst(Type type, Inst *op, AnnotSet &&annot)
    : UnaryInst(Kind::LOG, type, op, std::move(annot))
  {
  }
};

/**
 * Log2Inst
 */
class Log2Inst final : public UnaryInst {
public:
  Log2Inst(Type type, Inst *op, AnnotSet &&annot)
    : UnaryInst(Kind::LOG2, type, op, std::move(annot))
  {
  }
};

/**
 * Log10Inst
 */
class Log10Inst final : public UnaryInst {
public:
  Log10Inst(Type type, Inst *op, AnnotSet &&annot)
    : UnaryInst(Kind::LOG10, type, op, std::move(annot))
  {
  }
};

/**
 * FCeilInst
 */
class FCeilInst final : public UnaryInst {
public:
  FCeilInst(Type type, Inst *op, AnnotSet &&annot)
    : UnaryInst(Kind::FCEIL, type, op, std::move(annot))
  {
  }
};

/**
 * FFloorInst
 */
class FFloorInst final : public UnaryInst {
public:
  FFloorInst(Type type, Inst *op, AnnotSet &&annot)
    : UnaryInst(Kind::FFLOOR, type, op, std::move(annot))
  {
  }
};

/**
 * PopCountInst
 */
class PopCountInst final : public UnaryInst {
public:
  PopCountInst(Type type, Inst *op, AnnotSet &&annot)
    : UnaryInst(Kind::POPCNT, type, op, std::move(annot))
  {
  }
};

/**
 * CLZInst
 */
class CLZInst final : public UnaryInst {
public:
  CLZInst(Type type, Inst *op, AnnotSet &&annot)
    : UnaryInst(Kind::CLZ, type, op, std::move(annot))
  {
  }
};

/**
 * CTZInst
 */
class CTZInst final : public UnaryInst {
public:
  CTZInst(Type type, Inst *op, AnnotSet &&annot)
    : UnaryInst(Kind::CTZ, type, op, std::move(annot))
  {
  }
};

