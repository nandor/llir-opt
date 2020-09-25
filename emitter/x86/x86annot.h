// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <set>

#include <llvm/Pass.h>
#include <llvm/IR/DataLayout.h>

class Prog;
class MovInst;
class X86ISel;
class X86LVA;


/**
 * X86 Annotation Handler.
 */
class X86Annot final : public llvm::ModulePass {
public:
  static char ID;

  /// Initialises the pass which prints data sections.
  X86Annot(
      llvm::MCContext *ctx,
      llvm::MCStreamer *os,
      const llvm::MCObjectFileInfo *objInfo,
      const llvm::DataLayout &layout
  );

private:
  /// Creates MachineFunctions from LLIR IR.
  bool runOnModule(llvm::Module &M) override;
  /// Hardcoded name.
  llvm::StringRef getPassName() const override;
  /// Requires MachineModuleInfo.
  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;

private:
  /// Information about a call frame.
  struct FrameInfo {
    /// Label after a function call.
    llvm::MCSymbol *Label;
    /// Number of bytes allocated in the frame.
    int16_t FrameSize;
    /// Information about live offsets.
    std::set<uint16_t> Live;
    /// Allocation sizes.
    std::vector<size_t> Allocs;
  };

  /// Lowers a frameinfo structure.
  void LowerFrame(const FrameInfo &frame);
  /// Lowers a symbol name.
  llvm::MCSymbol *LowerSymbol(const std::string_view name);

private:
  /// Program being lowered.
  const Prog *prog_;
  /// Instruction selector pass containing info for annotations.
  const X86ISel *isel_;
  /// LLVM context.
  llvm::MCContext *ctx_;
  /// Streamer to emit output to.
  llvm::MCStreamer *os_;
  /// Object-file specific information.
  const llvm::MCObjectFileInfo *objInfo_;
  /// Data layout.
  const llvm::DataLayout &layout_;
  /// List of frames to emit information for.
  std::vector<FrameInfo> frames_;
  /// List of root frames.
  std::vector<llvm::MCSymbol *> roots_;
};
