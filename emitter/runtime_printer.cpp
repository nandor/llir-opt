// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/BinaryFormat/ELF.h>
#include <llvm/CodeGen/MachineModuleInfo.h>
#include <llvm/IR/Mangler.h>
#include <llvm/MC/MCSectionELF.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/data.h"
#include "core/func.h"
#include "core/insts/call.h"
#include "core/insts/const.h"
#include "core/prog.h"
#include "emitter/runtime_printer.h"

using MCSymbol = llvm::MCSymbol;
using MCInst = llvm::MCInst;
using MCOperand = llvm::MCOperand;



// -----------------------------------------------------------------------------
RuntimePrinter::RuntimePrinter(
    char &ID,
    const Prog &prog,
    const llvm::TargetMachine &tm,
    llvm::MCContext &ctx,
    llvm::MCStreamer &os,
    const llvm::MCObjectFileInfo &objInfo,
    bool shared)
  : llvm::ModulePass(ID)
  , prog_(prog)
  , tm_(tm)
  , ctx_(ctx)
  , os_(os)
  , objInfo_(objInfo)
  , layout_(tm.createDataLayout())
  , shared_(shared)
{
}

// -----------------------------------------------------------------------------
static bool NeedsCCall(const Prog &prog)
{
  for (const Func &func : prog) {
    for (const Block &block : func) {
      for (const Inst &inst : block) {
        if (!inst.HasAnnot<CamlFrame>()) {
          continue;
        }
        switch (inst.GetKind()) {
          case Inst::Kind::CALL:
          case Inst::Kind::TCALL:
          case Inst::Kind::INVOKE: {
            auto &site = static_cast<const CallSite &>(inst);
            if (site.GetCallingConv() == CallingConv::C) {
              return true;
            }
            break;
          }
          default: {
            llvm_unreachable("invalid @caml_frame annotation");
          }
        }
      }
    }
  }
  return false;
}

// -----------------------------------------------------------------------------
bool RuntimePrinter::runOnModule(llvm::Module &M)
{
  // Emit the OCaml runtime components.
  if (!shared_) {
    for (auto &ext : prog_.externs()) {
      if (ext.getName() == "caml_call_gc") {
        EmitCamlCallGc(*M.getFunction("caml_call_gc"));
      }
    }
    if (NeedsCCall(prog_)) {
      EmitCamlCCall(*M.getFunction("caml_c_call"));
    }
  }
  return false;
}

// -----------------------------------------------------------------------------
void RuntimePrinter::getAnalysisUsage(llvm::AnalysisUsage &AU) const
{
  AU.setPreservesAll();
  AU.addRequired<llvm::MachineModuleInfoWrapperPass>();
}
