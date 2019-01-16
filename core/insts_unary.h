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
  AbsInst(Type type, Inst *op)
    : UnaryInst(Kind::ABS, type, op)
  {
  }
};

/**
 * NegInst
 */
class NegInst final : public UnaryInst {
public:
  NegInst(Type type, Inst *op)
    : UnaryInst(Kind::NEG, type, op)
  {
  }
};

/**
 * SqrtInst
 */
class SqrtInst final : public UnaryInst {
public:
  SqrtInst(Type type, Inst *op)
    : UnaryInst(Kind::SQRT, type, op)
  {
  }
};

/**
 * SinInst
 */
class SinInst final : public UnaryInst {
public:
  SinInst(Type type, Inst *op)
    : UnaryInst(Kind::SIN, type, op)
  {
  }
};

/**
 * CosInst
 */
class CosInst final : public UnaryInst {
public:
  CosInst(Type type, Inst *op)
    : UnaryInst(Kind::COS, type, op)
  {
  }
};

/**
 * SExtInst
 */
class SExtInst final : public UnaryInst {
public:
  SExtInst(Type type, Inst *op)
    : UnaryInst(Kind::SEXT, type, op)
  {
  }
};

/**
 * ZExtInst
 */
class ZExtInst final : public UnaryInst {
public:
  ZExtInst(Type type, Inst *op)
    : UnaryInst(Kind::ZEXT, type, op)
  {
  }
};

/**
 * FExtInst
 */
class FExtInst final : public UnaryInst {
public:
  FExtInst(Type type, Inst *op)
    : UnaryInst(Kind::FEXT, type, op)
  {
  }
};

/**
 * TruncInst
 */
class TruncInst final : public UnaryInst {
public:
  TruncInst(Type type, Inst *op)
    : UnaryInst(Kind::TRUNC, type, op)
  {
  }
};
