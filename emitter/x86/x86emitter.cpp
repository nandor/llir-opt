// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/CodeGen/MachineFunctionPass.h>
#include <llvm/CodeGen/Passes.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/Target/TargetLoweringObjectFile.h>

#include "core/func.h"
#include "core/prog.h"
#include "core/visibility.h"
#include "emitter/x86/x86annot_printer.h"
#include "emitter/x86/x86isel.h"
#include "emitter/x86/x86emitter.h"
#include "emitter/x86/x86runtime_printer.h"

#define DEBUG_TYPE "llir-x86-isel-pass"



// -----------------------------------------------------------------------------
X86Emitter::X86Emitter(
    const std::string &path,
    llvm::raw_fd_ostream &os,
    X86Target &target)
  : Emitter(path, os, target)
  , x86target_(target)
  , TLII_(x86target_.GetTriple())
  , LibInfo_(TLII_)
{
}

// -----------------------------------------------------------------------------
X86Emitter::~X86Emitter()
{
}

// -----------------------------------------------------------------------------
ISel *X86Emitter::CreateISelPass(const Prog &prog, llvm::CodeGenOpt::Level opt)
{
  return new X86ISel(
      x86target_.GetTargetMachine(),
      LibInfo_,
      prog,
      llvm::CodeGenOpt::Aggressive,
      shared_
  );
}

// -----------------------------------------------------------------------------
AnnotPrinter *X86Emitter::CreateAnnotPass(
    llvm::MCContext &mcCtx,
    llvm::MCStreamer &mcStreamer,
    const llvm::TargetLoweringObjectFile &objInfo,
    ISel &isel)
{
  return new X86AnnotPrinter(
      &mcCtx,
      &mcStreamer,
      &objInfo,
      x86target_.GetTargetMachine().createDataLayout(),
      isel,
      shared_
  );
}

// -----------------------------------------------------------------------------
llvm::ModulePass *X86Emitter::CreateRuntimePass(
    const Prog &prog,
    llvm::MCContext &mcCtx,
    llvm::MCStreamer &mcStreamer,
    const llvm::TargetLoweringObjectFile &objInfo)
{
  return new X86RuntimePrinter(
    prog,
    x86target_.GetTargetMachine(),
    mcCtx,
    mcStreamer,
    objInfo,
    shared_
  );
}
