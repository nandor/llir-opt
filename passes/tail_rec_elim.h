// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/pass.h"

class Func;



/**
 * Tail recursion-to-iteration pass.
 *
 * Turns tail-recursive methods into iterative loops, enabling our optimiser
 * and LLVM to further improve them. The lowering of loops in OCaml is not
 * particularly effective, but its optimiser and code generator handle tail
 * recursion well. The opposite is true of LLVM and LLIR, which are based on
 * SSA: most optimisation passes target loops and prologue/epilogue insertion
 * is not optimal on tail-recursive methods. In addition, transforming tail
 * recursion into a loop aids register allocation, since arguments are no
 * longer fixed to specific registers at the point of the backwards jump.
 */
class TailRecElimPass final : public Pass {
public:
  /// Pass identifier.
  static const char *kPassID;

  /// Initialises the pass.
  TailRecElimPass(PassManager *passManager) : Pass(passManager) {}

  /// Runs the pass.
  void Run(Prog *prog) override;

  /// Returns the name of the pass.
  const char *GetPassName() const override;

private:
  /// Runs the pass on a function.
  void Run(Func &func);
};
