// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/pass.h"
#include "core/ref.h"

class Block;
class Func;
class JumpCondInst;
class CmpInst;



/**
 * Pass to bypass jumps through PHIs.
 */
class BypassPhiPass final : public Pass {
public:
  /// Pass identifier.
  static const char *kPassID;

  /// Initialises the pass.
  BypassPhiPass(PassManager *passManager) : Pass(passManager) {}

  /// Runs the pass.
  bool Run(Prog &prog) override;

  /// Returns the name of the pass.
  const char *GetPassName() const override;

private:
  /// Bypasses redundant comparisons involving PHIs.
  bool BypassPhiCmp(Block &block);
  /// Bypass with a candidate conditional.
  bool Bypass(
    JumpCondInst &jcc,
    CmpInst &cmp,
    Ref<Inst> phiCandidate,
    Ref<Inst> reference,
    Block &block
  );
};
