// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

class Prog;


/**
 * Abstract base class for program-altering passes.
 */
class Pass {
public:
  virtual ~Pass();

  /**
   * Runs the pass on a program.
   */
  virtual void Run(Prog *prog) = 0;
};
