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
  /// Analyses a function for arguments.
  AArch64Call(const Func *func)
    : CallLowering(func)
    , x_(0)
    , d_(0)
  {
    AnalyseFunc(func);
  }

  /// Analyses a call site.
  AArch64Call(const CallSite *call, bool isVarArg, bool isTailCall)
    : CallLowering(call)
  {
    AnalyseCall(call);
  }

  /// Returns the type of a return value.
  RetLoc Return(Type type) const override;

private:
  /// Location assignment for C calls.
  void AssignC(unsigned i, Type type, const Inst *value) override;
  /// Location assignment for Ocaml calls.
  void AssignOCaml(unsigned i, Type type, const Inst *value) override;
  /// Location assignment for OCaml to C allocator calls.
  void AssignOCamlAlloc(unsigned i, Type type, const Inst *value) override;
  /// Location assignment for OCaml to GC trampolines.
  void AssignOCamlGc(unsigned i, Type type, const Inst *value) override;

private:
  /// Number of arguments in integer registers.
  uint64_t x_;
  /// Number of arguments in floating-point registers.
  uint64_t d_;
};
