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

  /// Emits assembly for a program.
  virtual void EmitASM(const Prog *prog) = 0;

  /// Emits an object file for a program.
  virtual void EmitOBJ(const Prog *prog) = 0;
};
