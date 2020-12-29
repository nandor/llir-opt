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
  /**
   * Map an instruction producing a single value to a new value.
   *
   * @return True if the value changed.
   */
  bool Map(Inst &i, const SymbolicValue &value);

  /**
   * Return the value an instruction was mapped to.
   */
  SymbolicValue Lookup(ConstRef<Inst> inst);

private:
  /// Mapping from instruction sub-values to values.
  std::unordered_map<ConstRef<Inst>, SymbolicValue> values_;
};
