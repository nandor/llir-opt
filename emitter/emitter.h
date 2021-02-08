// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <fstream>
#include <string>

#include <llvm/Analysis/TargetLibraryInfo.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/Support/raw_ostream.h>

#include "core/target.h"

class Prog;
class ISel;
class AnnotPrinter;



/**
 * Generic emitter backend.
 */
class Emitter {
public:
  /// Creates an emitter.
  Emitter(
      const std::string &path,
      llvm::raw_fd_ostream &os,
      Target &target
  );

  /// Destroys the emitter.
  virtual ~Emitter();

  /// Emits assembly for a program.
  void EmitASM(const Prog &prog);

  /// Emits an object file for a program.
  void EmitOBJ(const Prog &prog);

private:
  /// Emits code using the LLVM pipeline.
  void Emit(llvm::CodeGenFileType type, const Prog &prog);

protected:
  /// Returns the generic target machine.
  virtual llvm::LLVMTargetMachine &GetTargetMachine() = 0;
  /// Creates the LLIR-to-SelectionDAG pass.
  virtual ISel *CreateISelPass(
      const Prog &prog,
      llvm::CodeGenOpt::Level opt
  ) = 0;
  /// Creates the annotation generation pass.
  virtual AnnotPrinter *CreateAnnotPass(
      llvm::MCContext &mcCtx,
      llvm::MCStreamer &mcStreamer,
      const llvm::TargetLoweringObjectFile &objInfo,
      ISel &isel
  ) = 0;
  /// Creates the runtime generation pass.
  virtual llvm::ModulePass *CreateRuntimePass(
      const Prog &prog,
      llvm::MCContext &mcCtx,
      llvm::MCStreamer &mcStreamer,
      const llvm::TargetLoweringObjectFile &objInfo
  ) = 0;

protected:
  /// Underlying target.
  Target &target_;
  /// Path to the output file.
  const std::string path_;
  /// Output stream.
  llvm::raw_fd_ostream &os_;
  /// Target triple.
  const std::string triple_;
  /// Flag to indicate if the target is a shared library.
  bool shared_;
  /// LLVM Context.
  llvm::LLVMContext context_;
};
