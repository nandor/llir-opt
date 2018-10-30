// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "emitter/emitter.h"



/**
 * Direct X86 emitter.
 */
class X86Emitter : public Emitter {
public:
  /// Creates an x86 emitter.
  X86Emitter();
  /// Destroys the x86 emitter.
  ~X86Emitter();

  /// Emits code for a program.
  void Emit(Prog *prog) override;
};
