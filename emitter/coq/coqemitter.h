// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <llvm/Support/raw_ostream.h>

class Prog;
class Func;


/**
 * Coq IR emitter
 */
class CoqEmitter final {
public:
  /// Creates a coq emitter.
  CoqEmitter(llvm::raw_ostream &os);

  /// Writes a program.
  void Write(const Prog &prog);

private:
  /// Writes a function.
  void Write(const Func &func);

private:
  /// Stream to write to.
  llvm::raw_ostream &os_;
};
