// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <iostream>
#include <cstdlib>

#include <llvm/Support/FileSystem.h>
#include <llvm/Support/InitLLVM.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/ToolOutputFile.h>

#include "core/parser.h"
#include "core/pass_manager.h"
#include "core/pass_registry.h"
#include "core/printer.h"
#include "emitter/x86/x86emitter.h"
#include "passes/dead_code_elim.h"
#include "passes/dead_func_elim.h"
#include "passes/dedup_block.h"
#include "passes/higher_order.h"
#include "passes/inliner.h"
#include "passes/local_const.h"
#include "passes/move_elim.h"
#include "passes/pta.h"
#include "passes/sccp.h"
#include "passes/rewriter.h"
#include "passes/simplify_cfg.h"
#include "passes/simplify_trampoline.h"
#include "passes/tail_rec_elim.h"
#include "passes/vtpta.h"
#include "stats/alloc_size.h"

namespace cl = llvm::cl;
namespace sys = llvm::sys;



/**
 * Enumeration of output formats.
 */
enum class OutputType {
  OBJ,
  ASM,
  LLIR
};

/**
 * Enumeration of optimisation levels.
 */
enum class OptLevel {
  /// No optimisations.
  O0,
  /// Simple optimisations.
  O1,
  /// Aggressive optimisations.
  O2,
  /// All optimisations.
  O3
};


// -----------------------------------------------------------------------------
static cl::opt<bool>
kVerbose("v", cl::desc("verbosity flag"), cl::Hidden);

static cl::opt<std::string>
kInput(cl::Positional, cl::desc("<input>"), cl::Required);

static cl::opt<std::string>
kOutput("o", cl::desc("output"), cl::init("-"));

static cl::opt<bool>
kTime("time", cl::desc("time passes"));

static cl::opt<bool>
kO0("O0", cl::desc("No optimisations"));

static cl::opt<bool>
kO1("O1", cl::desc("Simple optimisations"));

static cl::opt<bool>
kO2("O2", cl::desc("Aggressive optimisations"));

static cl::opt<bool>
kO3("O3", cl::desc("All optimisations"));

static cl::opt<std::string>
kTriple("triple", cl::desc("Override host target triple"));

static cl::opt<std::string>
kPasses("passes", cl::desc("specify a list of passes to run"));

static cl::opt<bool>
kS("S", cl::desc("Emit IR"));

// -----------------------------------------------------------------------------
static OptLevel GetOptLevel()
{
  if (kO3) {
    return OptLevel::O3;
  }
  if (kO2) {
    return OptLevel::O2;
  }
  if (kO1) {
    return OptLevel::O1;
  }
  return OptLevel::O0;
}

// -----------------------------------------------------------------------------
static void AddOpt0(PassManager &mngr)
{
  mngr.Add<RewriterPass>();
  mngr.Add<MoveElimPass>();
  mngr.Add<DeadCodeElimPass>();
}

// -----------------------------------------------------------------------------
static void AddOpt1(PassManager &mngr)
{
  mngr.Add<RewriterPass>();
  mngr.Add<MoveElimPass>();
  mngr.Add<DeadCodeElimPass>();
  mngr.Add<SimplifyCfgPass>();
  mngr.Add<TailRecElimPass>();
  mngr.Add<SimplifyTrampolinePass>();
  mngr.Add<InlinerPass>();
  mngr.Add<HigherOrderPass>();
  mngr.Add<InlinerPass>();
  mngr.Add<DeadFuncElimPass>();
  mngr.Add<SCCPPass>();
  mngr.Add<DedupBlockPass>();
  mngr.Add<SimplifyCfgPass>();
  mngr.Add<DeadCodeElimPass>();
  mngr.Add<DeadFuncElimPass>();
}

// -----------------------------------------------------------------------------
static void AddOpt2(PassManager &mngr)
{
  mngr.Add<RewriterPass>();
  mngr.Add<MoveElimPass>();
  mngr.Add<DeadCodeElimPass>();
  mngr.Add<SimplifyCfgPass>();
  mngr.Add<TailRecElimPass>();
  mngr.Add<SimplifyTrampolinePass>();
  mngr.Add<InlinerPass>();
  mngr.Add<HigherOrderPass>();
  mngr.Add<InlinerPass>();
  mngr.Add<DeadFuncElimPass>();
  mngr.Add<LocalConstPass>();
  mngr.Add<SCCPPass>();
  mngr.Add<DedupBlockPass>();
  mngr.Add<SimplifyCfgPass>();
  mngr.Add<DeadCodeElimPass>();
  mngr.Add<DeadFuncElimPass>();
}

// -----------------------------------------------------------------------------
static void AddOpt3(PassManager &mngr)
{
  mngr.Add<RewriterPass>();
  mngr.Add<MoveElimPass>();
  mngr.Add<DeadCodeElimPass>();
  mngr.Add<SimplifyCfgPass>();
  mngr.Add<TailRecElimPass>();
  mngr.Add<SimplifyTrampolinePass>();
  mngr.Add<InlinerPass>();
  mngr.Add<HigherOrderPass>();
  mngr.Add<InlinerPass>();
  mngr.Add<DeadFuncElimPass>();
  mngr.Add<LocalConstPass>();
  mngr.Add<SCCPPass>();
  mngr.Add<DedupBlockPass>();
  mngr.Add<SimplifyCfgPass>();
  mngr.Add<DeadCodeElimPass>();
  mngr.Add<PointsToAnalysis>();
  mngr.Add<DeadFuncElimPass>();
}

// -----------------------------------------------------------------------------
static std::unique_ptr<Emitter> GetEmitter(
    const std::string name,
    llvm::raw_fd_ostream &os,
    const llvm::Triple &triple)
{
  switch (triple.getArch()) {
    case llvm::Triple::x86_64: {
      return std::make_unique<X86Emitter>(name, os, triple.normalize());
    }
    default: {
      llvm::report_fatal_error("Unknown architecture: " + triple.normalize());
    }
  }
}

// -----------------------------------------------------------------------------
int main(int argc, char **argv)
{
  llvm::InitLLVM X(argc, argv);

  // Parse command line options.
  if (!llvm::cl::ParseCommandLineOptions(argc, argv, "LLIR optimiser\n\n")) {
    return EXIT_FAILURE;
  }

  // Initialise the relevant LLVM modules.
  llvm::InitializeAllTargetInfos();
  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmParsers();
  llvm::InitializeAllAsmPrinters();

  // Get the target triple to compile for.
  llvm::Triple triple;
  if (!kTriple.empty()) {
    triple = llvm::Triple(kTriple);
  } else {
    triple = llvm::Triple(llvm::sys::getDefaultTargetTriple());
  }

  // Open the input.
  auto FileOrErr = llvm::MemoryBuffer::getFileOrSTDIN(kInput);
  if (auto EC = FileOrErr.getError()) {
    llvm::errs() << "[Error] Cannot open input: " + EC.message();
    return EXIT_FAILURE;
  }

  // Parse the linked blob, optimise it and emit code.
  Parser parser(FileOrErr.get()->getMemBufferRef());
  if (auto *prog = parser.Parse()) {
    // Register all the passes.
    PassRegistry registry;
    registry.Register<AllocSizePass>();
    registry.Register<DeadCodeElimPass>();
    registry.Register<DeadFuncElimPass>();
    registry.Register<HigherOrderPass>();
    registry.Register<InlinerPass>();
    registry.Register<LocalConstPass>();
    registry.Register<MoveElimPass>();
    registry.Register<PointsToAnalysis>();
    registry.Register<SCCPPass>();
    registry.Register<SimplifyCfgPass>();
    registry.Register<TailRecElimPass>();
    registry.Register<VariantTypePointsToAnalysis>();
    registry.Register<SimplifyTrampolinePass>();
    registry.Register<DedupBlockPass>();
    registry.Register<RewriterPass>();

    // Set up the pipeline.
    PassManager passMngr(kVerbose, kTime);
    if (!kPasses.empty()) {
      llvm::SmallVector<llvm::StringRef, 3> passNames;
      llvm::StringRef(kPasses).split(passNames, ',', -1, false);
      for (auto &passName : passNames) {
        registry.Add(passMngr, passName);
      }
    } else {
      switch (GetOptLevel()) {
        case OptLevel::O0: AddOpt0(passMngr); break;
        case OptLevel::O1: AddOpt1(passMngr); break;
        case OptLevel::O2: AddOpt2(passMngr); break;
        case OptLevel::O3: AddOpt3(passMngr); break;
      }
    }

    // Run the optimiser.
    passMngr.Run(prog);

    // Determine the output type.
    if (kOutput != "/dev/null") {
      llvm::StringRef out = kOutput;
      OutputType type;
      bool isBinary;
      if (kS || out.endswith(".llir")) {
        type = OutputType::LLIR;
        isBinary = false;
      } else if (out.endswith(".S") || out.endswith(".s") || out == "-") {
        type = OutputType::ASM;
        isBinary = false;
      } else if (out.endswith(".o")) {
        type = OutputType::OBJ;
        isBinary = true;
      } else {
        llvm::errs() << "[Error] Invalid output format!\n";
        return EXIT_FAILURE;
      }

      // Open the output stream.
      std::error_code err;
      sys::fs::OpenFlags fs = isBinary ? sys::fs::F_Text : sys::fs::F_None;
      auto output = std::make_unique<llvm::ToolOutputFile>(kOutput, err, fs);
      if (err) {
        llvm::errs() << err.message() << "\n";
        return EXIT_FAILURE;
      }

      // Generate code.
      switch (type) {
        case OutputType::ASM: {
          GetEmitter(kInput, output->os(), triple)->EmitASM(prog);
          break;
        }
        case OutputType::OBJ: {
          GetEmitter(kInput, output->os(), triple)->EmitOBJ(prog);
          break;
        }
        case OutputType::LLIR: {
          Printer(output->os()).Print(prog);
          break;
        }
      }

      output->keep();
    }
    return EXIT_SUCCESS;
  }
  return EXIT_FAILURE;
}
