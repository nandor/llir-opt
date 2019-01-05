// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/Optional.h>
#include <llvm/CodeGen/AsmPrinter.h>
#include <llvm/CodeGen/MachineFunctionPass.h>
#include <llvm/CodeGen/MachineModuleInfo.h>
#include <llvm/CodeGen/Passes.h>
#include <llvm/CodeGen/SelectionDAG.h>
#include <llvm/CodeGen/TargetPassConfig.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/Target/TargetLoweringObjectFile.h>

#include "core/block.h"
#include "core/func.h"
#include "core/prog.h"
#include "emitter/data_printer.h"
#include "emitter/isel.h"
#include "emitter/x86/x86annot.h"
#include "emitter/x86/x86isel.h"
#include "emitter/x86/x86emitter.h"

#define DEBUG_TYPE "genm-isel-pass"

using namespace llvm;


// -----------------------------------------------------------------------------
class LambdaPass final : public llvm::ModulePass {
public:
  LambdaPass(std::function<void()> &&func)
    : llvm::ModulePass(*(new char()))
    , func_(std::forward<std::function<void()>>(func))
  {
  }

  bool runOnModule(llvm::Module &M) override
  {
    func_();
    return false;
  }

  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override
  {
    AU.setPreservesAll();
    AU.addRequired<llvm::MachineModuleInfo>();
  }

private:
  /// Function to invoke.
  std::function<void()> func_;
};


// -----------------------------------------------------------------------------
X86Emitter::X86Emitter(const std::string &path, llvm::raw_fd_ostream &os)
  : path_(path)
  , os_(os)
  , triple_("x86_64-apple-darwin13.4.0")
  , context_()
  , TLII_(Triple(triple_))
  , LibInfo_(TLII_)
{
  // Look up a backend for this target.
  std::string error;
  target_ = TargetRegistry::lookupTarget(triple_, error);
  if (!target_) {
    throw std::runtime_error(error);
  }

  // Initialise the target machine. Hacky cast to expose LLVMTargetMachine.
  TargetOptions opt;
  TM_ = static_cast<X86TargetMachine *>(
      target_->createTargetMachine(
          triple_,
          "generic",
          "",
          opt,
          Optional<Reloc::Model>(),
          CodeModel::Small,
          CodeGenOpt::Aggressive
      )
  );
  TM_->setFastISel(false);

  /// Initialise the subtarget.
  STI_ = new X86Subtarget(
      Triple(triple_),
      "",
      "",
      *TM_,
      0,
      0,
      UINT32_MAX
  );
}

// -----------------------------------------------------------------------------
X86Emitter::~X86Emitter()
{
}

// -----------------------------------------------------------------------------
void X86Emitter::Emit(TargetMachine::CodeGenFileType type, const Prog *prog)
{
  std::error_code errCode;
  legacy::PassManager passMngr;

  // Create a machine module info object.
  auto *MMI = new MachineModuleInfo(TM_);
  auto *MC = &MMI->getContext();
  auto dl = TM_->createDataLayout();

  // Create a target pass configuration.
  auto *passConfig = TM_->createPassConfig(passMngr);
  passMngr.add(passConfig);
  passMngr.add(MMI);

  auto *iSelPass = new X86ISel(
      TM_,
      STI_,
      STI_->getInstrInfo(),
      STI_->getRegisterInfo(),
      STI_->getTargetLowering(),
      &LibInfo_,
      prog,
      CodeGenOpt::Aggressive
  );

  passConfig->setDisableVerify(false);
  passConfig->addPass(iSelPass);
  passConfig->addMachinePasses();
  passConfig->setInitialized();


  // Create the assembly printer.
  auto *printer = TM_->createAsmPrinter(os_, nullptr, type, *MC);
  if (!printer) {
    throw std::runtime_error("Cannot create LLVM assembly printer");
  }
  auto *mcCtx = &printer->OutContext;
  auto *os = printer->OutStreamer.get();
  auto *objInfo = &printer->getObjFileLowering();

  // Emit assembly with a custom wrapper.
  auto emitValue = [&] (const std::string_view name) {
    os->SwitchSection(objInfo->getTextSection());
    auto *ptr = mcCtx->createTempSymbol();
    os->EmitLabel(ptr);

    os->SwitchSection(objInfo->getDataSection());
    os->EmitLabel(mcCtx->getOrCreateSymbol(name.data()));
    os->EmitSymbolValue(ptr, 8);
  };

  // Add the annotation expansion pass, after all optimisations.
  passMngr.add(new X86Annot(prog, iSelPass, mcCtx, os, objInfo));

  // Emit data segments, printing them directly.
  passMngr.add(new DataPrinter(prog, iSelPass, mcCtx, os, objInfo, dl));

  // Run the printer, emitting code.
  passMngr.add(new LambdaPass([&] { emitValue("_caml_code_begin"); }));
  passMngr.add(printer);
  passMngr.add(new LambdaPass([&] { emitValue("_caml_code_end"); }));

  // Add a pass to clean up memory.
  passMngr.add(createFreeMachineFunctionPass());

  // Create a dummy module.
  auto M = std::make_unique<Module>(path_, context_);
  M->setDataLayout(TM_->createDataLayout());

  // Run all passes and emit code.
  passMngr.run(*M);
  os_.flush();
}
