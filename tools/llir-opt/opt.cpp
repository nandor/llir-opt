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
#include <llvm/Support/Host.h>

#include "core/bitcode.h"
#include "core/pass_manager.h"
#include "core/pass_registry.h"
#include "core/printer.h"
#include "core/prog.h"
#include "core/util.h"
#include "emitter/coq/coqemitter.h"
#include "emitter/x86/x86emitter.h"
#include "passes/dead_code_elim.h"
#include "passes/dead_data_elim.h"
#include "passes/dead_func_elim.h"
#include "passes/dedup_block.h"
#include "passes/higher_order.h"
#include "passes/inliner.h"
#include "passes/local_const.h"
#include "passes/move_elim.h"
#include "passes/pre_eval.h"
#include "passes/pta.h"
#include "passes/rewriter.h"
#include "passes/sccp.h"
#include "passes/simplify_cfg.h"
#include "passes/simplify_trampoline.h"
#include "passes/stack_object_elim.h"
#include "passes/tail_rec_elim.h"
#include "passes/undef_elim.h"
#include "passes/verifier.h"
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
  COQ,
  LLIR,
  LLBC,
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
  O3,
  /// Optimise for size.
  Os,
};

// -----------------------------------------------------------------------------
static cl::opt<bool>
optVerbose("v", cl::desc("verbosity flag"), cl::Hidden);

static cl::opt<std::string>
optInput(cl::Positional, cl::desc("<input>"), cl::Required);

static cl::opt<std::string>
optOutput("o", cl::desc("output"), cl::init("-"));

static cl::opt<bool>
optTime("time", cl::desc("time passes"));

static cl::opt<OptLevel>
optOptLevel(
  cl::desc("optimisation level:"),
  cl::values(
    clEnumValN(OptLevel::O0, "O0", "No optimizations"),
    clEnumValN(OptLevel::O1, "O1", "Simple optimisations"),
    clEnumValN(OptLevel::O2, "O2", "Aggressive optimisations"),
    clEnumValN(OptLevel::O3, "O3", "All optimisations"),
    clEnumValN(OptLevel::Os, "Os", "Optimise for size")
  ),
  cl::init(OptLevel::O0)
);

static cl::opt<std::string>
optTriple("triple", cl::desc("Override host target triple"));

static cl::opt<std::string>
optCPU("mcpu", cl::desc("Override the host CPU"));

static cl::opt<std::string>
optTuneCPU("mtune", cl::desc("Override the tune CPU"));

static cl::opt<std::string>
optPasses("passes", cl::desc("specify a list of passes to run"));

static cl::opt<OutputType>
optEmit("emit", cl::desc("Emit text-based LLIR"),
  cl::values(
    clEnumValN(OutputType::OBJ,  "obj",  "target-specific object file"),
    clEnumValN(OutputType::ASM,  "asm",  "x86 object file"),
    clEnumValN(OutputType::COQ,  "coq",  "Coq IR"),
    clEnumValN(OutputType::LLIR, "llir", "LLIR text file"),
    clEnumValN(OutputType::LLBC, "llbc", "LLIR binary file")
  ),
  cl::Optional
);

static cl::opt<bool>
optShared("shared", cl::desc("Compile for a shared library"), cl::init(false));



// -----------------------------------------------------------------------------
static void AddOpt0(PassManager &mngr)
{
  mngr.Add<VerifierPass>();
}

// -----------------------------------------------------------------------------
static void AddOpt1(PassManager &mngr)
{
  mngr.Add<VerifierPass>();
  mngr.Add<RewriterPass>();
  mngr.Add<MoveElimPass>();
  mngr.Add<DeadCodeElimPass>();
  mngr.Add<SimplifyCfgPass>();
  mngr.Add<TailRecElimPass>();
  mngr.Add<SimplifyTrampolinePass>();
  mngr.Add<DeadFuncElimPass>();
  mngr.Add<SCCPPass>();
  mngr.Add<DedupBlockPass>();
  mngr.Add<SimplifyCfgPass>();
  mngr.Add<DeadCodeElimPass>();
  mngr.Add<StackObjectElimPass>();
  mngr.Add<DeadFuncElimPass>();
  mngr.Add<DeadDataElimPass>();
  mngr.Add<VerifierPass>();
}

// -----------------------------------------------------------------------------
static void AddOpt2(PassManager &mngr)
{
  mngr.Add<VerifierPass>();
  /*
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
  mngr.Add<StackObjectElimPass>();
  mngr.Add<DeadFuncElimPass>();
  mngr.Add<DeadDataElimPass>();
  mngr.Add<VerifierPass>();
  */
}

// -----------------------------------------------------------------------------
static void AddOpt3(PassManager &mngr)
{
  mngr.Add<VerifierPass>();
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
  mngr.Add<StackObjectElimPass>();
  mngr.Add<PointsToAnalysis>();
  mngr.Add<DeadFuncElimPass>();
  mngr.Add<DeadDataElimPass>();
  mngr.Add<VerifierPass>();
}

// -----------------------------------------------------------------------------
int main(int argc, char **argv)
{
  llvm::InitLLVM X(argc, argv);

  // Parse command line options.
  if (!llvm::cl::ParseCommandLineOptions(argc, argv, "LLIR optimiser\n")) {
    return EXIT_FAILURE;
  }

  // Initialise the relevant LLVM modules.
  llvm::InitializeAllTargetInfos();
  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmPrinters();

  // Get the target triple to compile for.
  llvm::Triple triple;
  if (!optTriple.empty()) {
    triple = llvm::Triple(optTriple);
  } else {
    triple = llvm::Triple(llvm::sys::getDefaultTargetTriple());
  }
  // Find the CPU to compile for.
  std::string CPU;
  if (optCPU.empty()) {
    CPU = std::string(llvm::sys::getHostCPUName());
  } else {
    CPU = optCPU;
  }
  // Process the tune argument.
  std::string tuneCPU = optTuneCPU.empty() ? CPU : optTuneCPU;

  // Open the input.
  auto FileOrErr = llvm::MemoryBuffer::getFileOrSTDIN(optInput);
  if (auto EC = FileOrErr.getError()) {
    llvm::errs() << "[Error] Cannot open input: " + EC.message();
    return EXIT_FAILURE;
  }

  // Parse the linked blob: if file starts with magic, parse bitcode.
  auto buffer = FileOrErr.get()->getMemBufferRef().getBuffer();
  std::unique_ptr<Prog> prog(Parse(buffer, abspath(optInput)));
  if (!prog) {
    return EXIT_FAILURE;
  }

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
  registry.Register<DeadDataElimPass>();
  registry.Register<UndefElimPass>();
  registry.Register<StackObjectElimPass>();
  registry.Register<PreEvalPass>();

  // Set up the pipeline.
  PassManager passMngr(optVerbose, optTime);
  if (!optPasses.empty()) {
    llvm::SmallVector<llvm::StringRef, 3> passNames;
    llvm::StringRef(optPasses).split(passNames, ',', -1, false);
    for (auto &passName : passNames) {
      registry.Add(passMngr, std::string(passName));
    }
  } else {
    switch (optOptLevel) {
      case OptLevel::O0: AddOpt0(passMngr); break;
      case OptLevel::O1: AddOpt1(passMngr); break;
      case OptLevel::O2: AddOpt2(passMngr); break;
      case OptLevel::O3: AddOpt3(passMngr); break;
      case OptLevel::Os: AddOpt1(passMngr); break;
    }
  }

  // Determine the output type.
  llvm::StringRef out = optOutput;

  // Figure out the output type.
  OutputType type;
  if (optEmit.getNumOccurrences()) {
    type = optEmit;
  } else if (out.endswith(".llir")) {
    type = OutputType::LLIR;
  } else if (out.endswith(".llbc")) {
    type = OutputType::LLBC;
  } else if (out.endswith(".S") || out.endswith(".s") || out == "-") {
    type = OutputType::ASM;
  } else if (out.endswith(".o")) {
    type = OutputType::OBJ;
  } else if (out.endswith(".v")) {
    type = OutputType::COQ;
  } else {
    llvm::errs() << "[Error] Unknown output format\n";
    return EXIT_FAILURE;
  }

  // Check if output is binary.
  // Add DCE and move elimination if code is generatoed.
  bool isBinary;
  switch (type) {
    case OutputType::ASM: {
      passMngr.Add<MoveElimPass>();
      passMngr.Add<DeadCodeElimPass>();
      isBinary = false;
      break;
    }
    case OutputType::OBJ: {
      passMngr.Add<MoveElimPass>();
      passMngr.Add<DeadCodeElimPass>();
      isBinary = true;
      break;
    }
    case OutputType::LLIR: {
      isBinary = false;
      break;
    }
    case OutputType::LLBC: {
      isBinary = true;
      break;
    }
    case OutputType::COQ: {
      isBinary = false;
      break;
    }
  }

  // Run the optimiser.
  passMngr.Run(*prog);

  // Open the output stream.
  std::error_code err;
  sys::fs::OpenFlags fs = isBinary ? sys::fs::F_None : sys::fs::F_Text;
  auto output = std::make_unique<llvm::ToolOutputFile>(optOutput, err, fs);
  if (err) {
    llvm::errs() << err.message() << "\n";
    return EXIT_FAILURE;
  }

  // Helper to create an emitter.
  auto getEmitter = [&] {
    switch (triple.getArch()) {
      case llvm::Triple::x86_64: {
        return std::make_unique<X86Emitter>(
            optInput,
            output->os(),
            triple.normalize(),
            CPU,
            tuneCPU,
            optShared
        );
      }
      default: {
        llvm::report_fatal_error("Unknown architecture: " + triple.normalize());
      }
    }
  };

  // Generate code.
  switch (type) {
    case OutputType::ASM: {
      getEmitter()->EmitASM(*prog);
      break;
    }
    case OutputType::OBJ: {
      getEmitter()->EmitOBJ(*prog);
      break;
    }
    case OutputType::LLIR: {
      Printer(output->os()).Print(*prog);
      break;
    }
    case OutputType::LLBC: {
      BitcodeWriter(output->os()).Write(*prog);
      break;
    }
    case OutputType::COQ: {
      CoqEmitter(output->os()).Write(*prog);
      break;
    }
  }

  output->keep();
  return EXIT_SUCCESS;
}
