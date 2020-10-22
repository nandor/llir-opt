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
  SelectInst(Type type,
      Ref<Inst> cond,
      Ref<Inst> vt,
      Ref<Inst> vf,
      AnnotSet &&annot
  );
  SelectInst(
      Type type,
      Ref<Inst> cond,
      Ref<Inst> vt,
      Ref<Inst> vf,
      const AnnotSet &annot
  );

  ConstRef<Inst> GetCond() const;
  Ref<Inst> GetCond();

  ConstRef<Inst> GetTrue() const;
  Ref<Inst> GetTrue();

  ConstRef<Inst> GetFalse() const;
  Ref<Inst> GetFalse();

  /// Instruction is not constant.
  bool IsConstant() const override { return false; }
};
