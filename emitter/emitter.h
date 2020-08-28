// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <llvm/Target/TargetMachine.h>

class Prog;



/**
 * Generic emitter backend.
 */
class Emitter {
public:
  /// Destroys the emitter.
  virtual ~Emitter();

  /// Emits assembly for a program.
  void EmitASM(const Prog &prog);

  /// Emits an object file for a program.
  void EmitOBJ(const Prog &prog);

private:
  virtual void Emit(llvm::CodeGenFileType type, const Prog &prog) = 0;
};
