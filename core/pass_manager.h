// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <vector>

class Pass;
class Prog;



/**
 * Pass manager, scheduling passes.
 */
class PassManager final {
public:
  PassManager(bool verbose, bool time);

  /// Adds a pass to the pipeline.
  void Add(Pass *pass);
  /// Runs the pipeline.
  void Run(Prog *prog);

private:
  /// Verbosity flag.
  bool verbose_;
  /// Timing flag.
  bool time_;
  /// List of passes to run on a program.
  std::vector<Pass *> passes_;
};
