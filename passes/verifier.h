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

  /// Return the pointer type.
  Type GetPointerType() const { return Type::I64; }

  /// Report an error.
  [[noreturn]] void Error(Inst &i, const char *msg);
};
