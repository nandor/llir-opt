// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/pass.h"

class Func;
class MovInst;



/**
 * Pass to eliminate unnecessary moves.
 */
class VerifierPass final : public Pass {
public:
  /// Pass identifier.
  static const char *kPassID;

  /// Initialises the pass.
  VerifierPass(PassManager *passManager) : Pass(passManager) {}

  /// Runs the pass.
  void Run(Prog *prog) override;

  /// Returns the name of the pass.
  const char *GetPassName() const override;

private:
  /// Verifies a function.
  void Verify(Func &func);
  /// Verifies an instruction.
  void Verify(Inst &i);

  /// Ensure a type is an integer.
  void CheckInteger(
      const Inst &inst,
      Ref<Inst> ref,
      const char *msg = "not an integer"
  );

  /// Ensure a type is a pointer.
  void CheckPointer(
      const Inst &inst,
      Ref<Inst> ref,
      const char *msg = "not a pointer"
  );

  /// Ensure a type is compatible with a given one.
  void CheckType(
      const Inst &inst,
      Ref<Inst> ref,
      Type type
  );

  /// Return the pointer type.
  Type GetPointerType() const { return Type::I64; }

  /// Report an error.
  [[noreturn]] void Error(const Inst &i, const char *msg);
};
