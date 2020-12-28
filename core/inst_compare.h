// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/insts.h"



/**
 * Helper class to build instruction-to-instruction comparisons.
 */
class InstCompare {
public:
  virtual bool Equal(ConstRef<Value> a, ConstRef<Value> b) const;
  virtual bool Equal(ConstRef<Global> a, ConstRef<Global> b) const;
  virtual bool Equal(ConstRef<Expr> a, ConstRef<Expr> b) const;
  virtual bool Equal(ConstRef<Constant> a, ConstRef<Constant> b) const;
  virtual bool Equal(ConstRef<Inst> a, ConstRef<Inst> b) const;
  virtual bool Equal(const Block *a, const Block *b) const;

  bool IsEqual(const Inst &a, const Inst &b) const;
};
