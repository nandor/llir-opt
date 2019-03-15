// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "insts.h"



/**
 * AbsInst
 */
class AbsInst final : public UnaryInst {
public:
  AbsInst(Type type, Inst *op, const AnnotSet &annot)
    : UnaryInst(Kind::ABS, type, op, annot)
  {
  }
};

/**
 * NegInst
 */
class NegInst final : public UnaryInst {
public:
  NegInst(Type type, Inst *op, const AnnotSet &annot)
    : UnaryInst(Kind::NEG, type, op, annot)
  {
  }
};

/**
 * SqrtInst
 */
class SqrtInst final : public UnaryInst {
public:
  SqrtInst(Type type, Inst *op, const AnnotSet &annot)
    : UnaryInst(Kind::SQRT, type, op, annot)
  {
  }
};

/**
 * SinInst
 */
class SinInst final : public UnaryInst {
public:
  SinInst(Type type, Inst *op, const AnnotSet &annot)
    : UnaryInst(Kind::SIN, type, op, annot)
  {
  }
};

/**
 * CosInst
 */
class CosInst final : public UnaryInst {
public:
  CosInst(Type type, Inst *op, const AnnotSet &annot)
    : UnaryInst(Kind::COS, type, op, annot)
  {
  }
};

/**
 * SExtInst
 */
class SExtInst final : public UnaryInst {
public:
  SExtInst(Type type, Inst *op, const AnnotSet &annot)
    : UnaryInst(Kind::SEXT, type, op, annot)
  {
  }
};

/**
 * ZExtInst
 */
class ZExtInst final : public UnaryInst {
public:
  ZExtInst(Type type, Inst *op, const AnnotSet &annot)
    : UnaryInst(Kind::ZEXT, type, op, annot)
  {
  }
};

/**
 * FExtInst
 */
class FExtInst final : public UnaryInst {
public:
  FExtInst(Type type, Inst *op, const AnnotSet &annot)
    : UnaryInst(Kind::FEXT, type, op, annot)
  {
  }
};

/**
 * TruncInst
 */
class TruncInst final : public UnaryInst {
public:
  TruncInst(Type type, Inst *op, const AnnotSet &annot)
    : UnaryInst(Kind::TRUNC, type, op, annot)
  {
  }
};
