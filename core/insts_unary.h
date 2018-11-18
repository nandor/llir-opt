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
  AbsInst(Block *block, Type type, Inst *op)
    : UnaryInst(Kind::ABS, block, type, op)
  {
  }
};

/**
 * NegInst
 */
class NegInst final : public UnaryInst {
public:
  NegInst(Block *block, Type type, Inst *op)
    : UnaryInst(Kind::NEG, block, type, op)
  {
  }
};

/**
 * SqrtInst
 */
class SqrtInst final : public UnaryInst {
public:
  SqrtInst(Block *block, Type type, Inst *op)
    : UnaryInst(Kind::SQRT, block, type, op)
  {
  }
};

/**
 * SinInst
 */
class SinInst final : public UnaryInst {
public:
  SinInst(Block *block, Type type, Inst *op)
    : UnaryInst(Kind::SIN, block, type, op)
  {
  }
};

/**
 * CosInst
 */
class CosInst final : public UnaryInst {
public:
  CosInst(Block *block, Type type, Inst *op)
    : UnaryInst(Kind::COS, block, type, op)
  {
  }
};

/**
 * SExtInst
 */
class SExtInst final : public UnaryInst {
public:
  SExtInst(Block *block, Type type, Inst *op)
    : UnaryInst(Kind::SEXT, block, type, op)
  {
  }
};

/**
 * ZExtInst
 */
class ZExtInst final : public UnaryInst {
public:
  ZExtInst(Block *block, Type type, Inst *op)
    : UnaryInst(Kind::ZEXT, block, type, op)
  {
  }
};

/**
 * FExtInst
 */
class FExtInst final : public UnaryInst {
public:
  FExtInst(Block *block, Type type, Inst *op)
    : UnaryInst(Kind::FEXT, block, type, op)
  {
  }
};

/**
 * TruncInst
 */
class TruncInst final : public UnaryInst {
public:
  TruncInst(Block *block, Type type, Inst *op)
    : UnaryInst(Kind::TRUNC, block, type, op)
  {
  }
};
