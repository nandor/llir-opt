// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/inst.h"



/**
 * SelectInst
 */
class SelectInst final : public OperatorInst {
public:
  SelectInst(Type type, Inst *cond, Inst *vt, Inst *vf, AnnotSet &&annot);
  SelectInst(Type type, Inst *cond, Inst *vt, Inst *vf, const AnnotSet &annot);

  Inst *GetCond() const { return static_cast<Inst *>(Op<0>().get()); }
  Inst *GetTrue() const { return static_cast<Inst *>(Op<1>().get()); }
  Inst *GetFalse() const { return static_cast<Inst *>(Op<2>().get()); }

  /// Instruction is not constant.
  bool IsConstant() const override { return false; }
};
