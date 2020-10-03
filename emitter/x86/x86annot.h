// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <set>

#include <llvm/Pass.h>
#include <llvm/IR/DataLayout.h>

#include "core/adt/hash.h"

class Prog;
class MovInst;
class X86ISel;
class ISelMapping;



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
      const llvm::DataLayout &layout,
      const ISelMapping &mapping,
      bool shared
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
    /// Debug info symbols.
    std::vector<llvm::MCSymbol *> Debug;
  };

  /// Debug information key.
  struct DebugKey {
    /// Bundle of debug infos.
    const CamlFrame::DebugInfos &Debug;

    bool operator==(const DebugKey &that) const {
      return Debug == that.Debug;
    }
  };

  /// Debug information hasher.
  struct DebugKeyHash {
    size_t operator() (const DebugKey &key) const {
      size_t hash = 0;
      for (const auto &debug : key.Debug) {
        ::hash_combine(hash, std::hash<int64_t>{}(debug.Location));
        ::hash_combine(hash, std::hash<std::string>{}(debug.File));
        ::hash_combine(hash, std::hash<std::string>{}(debug.Definition));
      }
      return hash;
    }
  };

  /// Debug value.
  struct DebugInfo {
    llvm::MCSymbol *Definition;
    int64_t Location;
  };

  /// Debug value group.
  struct DebugInfos {
    llvm::MCSymbol *Symbol;
    std::vector<DebugInfo> Debug;
  };

  /// Definition.
  struct DefinitionInfo {
    llvm::MCSymbol *Symbol;
    llvm::MCSymbol *File;
    std::string Definition;
  };

  /// Lowers a frameinfo structure.
  void LowerFrame(const FrameInfo &frame);
  /// Lowers a symbol name.
  llvm::MCSymbol *LowerSymbol(const std::string_view name);
  /// Records a debug info object.
  llvm::MCSymbol *RecordDebug(const CamlFrame::DebugInfos &debug);
  /// Record a definition.
  llvm::MCSymbol *RecordDefinition(
      const std::string &file,
      const std::string &def
  );
  /// Record a file name.
  llvm::MCSymbol *RecordFile(const std::string &file);
  /// Emits a value which is relative to the current address.
  void EmitDiff(llvm::MCSymbol *symbol, unsigned size = 4);

private:
  /// Instruction selector pass containing info for annotations.
  const ISelMapping &mapping_;
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
  /// Mapping of debug objects.
  std::unordered_map<DebugKey, DebugInfos, DebugKeyHash> debug_;
  /// Mapping from definitions to labels.
  std::unordered_map<
    std::pair<std::string, std::string>,
    DefinitionInfo
  > defs_;
  /// Mapping from file names to labels.
  std::unordered_map<std::string, llvm::MCSymbol *> files_;
  /// Flag to indicate whether a shared library is emitted.
  bool shared_;
};
