// This file if part of the llir-opt project.
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
#include <llvm/IR/Mangler.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/Target/TargetLoweringObjectFile.h>

#include "core/prog.h"
#include "emitter/annot_printer.h"
#include "emitter/data_printer.h"
#include "emitter/emitter.h"
#include "emitter/isel.h"



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
    AU.addRequired<llvm::MachineModuleInfoWrapperPass>();
  }

private:
  /// Function to invoke.
  std::function<void()> func_;
};

// -----------------------------------------------------------------------------
Emitter::Emitter(
    const std::string &path,
    llvm::raw_fd_ostream &os,
    const std::string &triple,
    bool shared)
  : path_(path)
  , os_(os)
  , triple_(triple)
  , shared_(shared)
{
}

// -----------------------------------------------------------------------------
Emitter::~Emitter()
{
}


// -----------------------------------------------------------------------------
void Emitter::EmitASM(const Prog &prog)
{
  Emit(llvm::CodeGenFileType::CGFT_AssemblyFile, prog);
}

// -----------------------------------------------------------------------------
void Emitter::EmitOBJ(const Prog &prog)
{
  Emit(llvm::CodeGenFileType::CGFT_ObjectFile, prog);
}

// -----------------------------------------------------------------------------
void Emitter::Emit(llvm::CodeGenFileType type, const Prog &prog)
{
  std::error_code errCode;
  llvm::legacy::PassManager passMngr;

  // Create a machine module info object.
  auto &TM = GetTargetMachine();
  auto *MMIWP = new llvm::MachineModuleInfoWrapperPass(&TM);
  auto *MC = &MMIWP->getMMI().getContext();
  auto dl = TM.createDataLayout();

  // Create a dummy module.
  auto M = std::make_unique<llvm::Module>(path_, context_);
  M->setDataLayout(dl);

  {
    // Create a target pass configuration.
    auto *passConfig = TM.createPassConfig(passMngr);
    passMngr.add(passConfig);
    passMngr.add(MMIWP);

    auto *iSelPass = CreateISelPass(prog, llvm::CodeGenOpt::Aggressive);
    passConfig->setDisableVerify(false);
    passConfig->addPass(iSelPass);
    passConfig->addPass(&llvm::FinalizeISelID);
    passConfig->addMachinePasses();
    passConfig->setInitialized();

    // Create the assembly printer.
    auto *printer = TM.createAsmPrinter(os_, nullptr, type, *MC);
    if (!printer) {
      llvm::report_fatal_error("Cannot create LLVM assembly printer");
    }
    auto *mcCtx = &printer->OutContext;
    auto *os = printer->OutStreamer.get();
    auto *objInfo = &printer->getObjFileLowering();

    // Check if there are OCaml functions.
    bool hasOCaml = false;
    for (auto &func : prog) {
      if (func.GetCallingConv() == CallingConv::CAML) {
        hasOCaml = true;
        break;
      }
    }

    // Emit assembly with a custom wrapper.
    auto emitSymbol = [&, this] (const std::string_view name) {
      if (!hasOCaml) {
        return;
      }

      auto *prefix = shared_ ? "caml_shared_startup__code" : "caml__code";
      llvm::SmallString<128> mangledName;
      llvm::Mangler::getNameWithPrefix(
          mangledName,
          std::string(prefix) + std::string(name),
          dl
      );

      os->SwitchSection(objInfo->getTextSection());
      auto *sym = mcCtx->getOrCreateSymbol(mangledName);
      if (shared_) {
        os->emitSymbolAttribute(sym, llvm::MCSA_Global);
      }
      os->emitLabel(sym);
    };

    // Add the annotation expansion pass, after all optimisations.
    passMngr.add(CreateAnnotPass(*mcCtx, *os, *objInfo, *iSelPass));

    // Emit data segments, printing them directly.
    passMngr.add(new DataPrinter(
        prog,
        iSelPass,
        mcCtx,
        os,
        objInfo,
        dl,
        shared_
    ));
    // Emit the runtime component, printing them directly.
    passMngr.add(CreateRuntimePass(
        prog,
        *mcCtx,
        *os,
        *objInfo
    ));

    // Run the printer, emitting code.
    passMngr.add(new LambdaPass([&emitSymbol] { emitSymbol("_begin"); }));
    passMngr.add(printer);
    passMngr.add(new LambdaPass([&emitSymbol] { emitSymbol("_end"); }));

    // Add a pass to clean up memory.
    passMngr.add(llvm::createFreeMachineFunctionPass());

    // Run all passes and emit code.
    passMngr.run(*M);
  }
  os_.flush();
}
