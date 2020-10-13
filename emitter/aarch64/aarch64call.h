// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <vector>

#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/iterator_range.h>

#include "core/inst.h"
#include "emitter/call_lowering.h"

class CallInst;
class InvokeInst;
class TailCallInst;
class ReturnInst;
class Func;



/**
 * AArch64 calling convention classification.
 */
class AArch64Call final : public CallLowering {
public:

public:
  /// Returns the location where an argument is available on entry.
  const Loc &operator [] (size_t idx) const override;
};
