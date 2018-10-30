// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <fstream>
#include <string>
#include "emitter/emitter.h"

class Func;



/**
 * Direct X86 emitter.
 */
class X86Emitter : public Emitter {
public:
  /// Creates an x86 emitter.
  X86Emitter(const std::string &out);
  /// Destroys the x86 emitter.
  ~X86Emitter();

  /// Emits code for a program.
  void Emit(const Prog *prog) override;

private:
  /// Emits code for a function.
  void Emit(const Func *func);

private:
  /// Output stream.
  std::ofstream os_;
};
