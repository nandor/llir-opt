// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

class Pass;
class Prog;



/**
 * Pass manager, scheduling passes.
 */
class PassManager final {
public:
  PassManager();

  /// Adds a pass to the pipeline.
  void Add(Pass *pass);
  /// Runs the pipeline.
  void Run(Prog *prog);
};
