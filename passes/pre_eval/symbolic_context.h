// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/inst.h"
#include "passes/pre_eval/symbolic_value.h"



/**
 * Context for symbolic execution.
 */
class SymbolicContext final {
public:
  void Map(Inst &i, const SymbolicValue &v);

  SymbolicValue Lookup(ConstRef<Inst> inst);

private:
  std::unordered_map<ConstRef<Inst>, SymbolicValue> values_;
};
