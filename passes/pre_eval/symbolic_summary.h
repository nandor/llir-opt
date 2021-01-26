// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <unordered_map>

#include "core/ref.h"
#include "passes/pre_eval/symbolic_value.h"



/**
 * Helper class to record the targets of all instructions.
 */
class SymbolicSummary final {
public:
  SymbolicValue Lookup(ConstRef<Inst> ref);

  SymbolicValue Lookup(CallSite *site);

  void Map(ConstRef<Inst> ref, const SymbolicValue &value);

private:
  /// Mapping from instructions to the LUB of all values.
  std::unordered_map<ConstRef<Inst>, SymbolicValue> values_;
};
