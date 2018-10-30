// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

class Prog;



/**
 * Generic emitter backend.
 */
class Emitter {
public:
  /// Destroys the emitter.
  virtual ~Emitter();

  /// Emits code for a program.
  virtual void Emit(const Prog *prog) = 0;
};
