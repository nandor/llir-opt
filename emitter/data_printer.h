// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <map>

#include <llvm/Pass.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/MC/MCObjectFileInfo.h>
#include <llvm/MC/MCStreamer.h>

#include "core/visibility.h"

class Prog;
class Data;
class Atom;
class Object;
class ISelMapping;



/**
 * Pass to print all data segments.
 */
class DataPrinter final : public llvm::ModulePass {
public:
  static char ID;

  /// Initialises the pass which prints data sections.
  DataPrinter(
      const Prog &Prog,
      ISelMapping *isel,
      llvm::MCContext *ctx,
      llvm::MCStreamer *os,
      const llvm::MCObjectFileInfo *objInfo,
      const llvm::DataLayout &layout,
      bool shared
  );

private:
  /// Creates MachineFunctions from LLIR.
  bool runOnModule(llvm::Module &M) override;
  /// Hardcoded name.
  llvm::StringRef getPassName() const override;
  /// Requires MachineModuleInfo.
  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;

private:
  /// Lower an extern.
  void LowerExtern(const Extern &ext);
  /// Prints a section.
  void LowerSection(const Data &data);
  /// Prints an object.
  void LowerObject(const Object &object);
  /// Prints an atom.
  void LowerAtom(const Atom &atom);
  /// Lowers a symbol name.
  llvm::MCSymbol *LowerSymbol(const std::string_view name);
  /// Emits visibility attributes.
  void EmitVisibility(llvm::MCSymbol *sym, Visibility visibility);

  /// Helper classifying ctors/dtors by priority.
  using XtorMap = std::map<int, std::vector<const Func *>>;
  /// Lowers the xtors into the appropriate section.
  void LowerXtors(const XtorMap &map, const std::string &name);

private:
  /// Section for .data caml
  llvm::MCSection *GetSection(const Data &data);

private:
  /// Program to print.
  const Prog &prog_;
  /// Instruction selector state.
  ISelMapping *isel_;
  /// LLVM context.
  llvm::MCContext *ctx_;
  /// Streamer to emit output to.
  llvm::MCStreamer *os_;
  /// Object-file specific information.
  const llvm::MCObjectFileInfo *objInfo_;
  /// Data layout.
  const llvm::DataLayout &layout_;
  /// Flag to indicate if a shared library is being emitted.
  bool shared_;
};
