// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/BinaryFormat/ELF.h>
#include <llvm/CodeGen/MachineModuleInfo.h>
#include <llvm/IR/Mangler.h>
#include <llvm/MC/MCSectionELF.h>

#include "core/cast.h"
#include "core/data.h"
#include "core/prog.h"
#include "core/block.h"
#include "core/func.h"
#include "core/insts/const.h"
#include "emitter/runtime_printer.h"

using MCSymbol = llvm::MCSymbol;
using MCInst = llvm::MCInst;
using MCOperand = llvm::MCOperand;



// -----------------------------------------------------------------------------
RuntimePrinter::RuntimePrinter(
    char &ID,
    const Prog &prog,
    llvm::MCContext *ctx,
    llvm::MCStreamer *os,
    const llvm::MCObjectFileInfo *objInfo,
    const llvm::DataLayout &layout,
    bool shared)
  : llvm::ModulePass(ID)
  , prog_(prog)
  , ctx_(ctx)
  , os_(os)
  , objInfo_(objInfo)
  , layout_(layout)
  , shared_(shared)
{
}

// -----------------------------------------------------------------------------
bool RuntimePrinter::runOnModule(llvm::Module &)
{
  // Emit the OCaml runtime components.
  if (!shared_) {
    {
      bool needsCallGC = false;
      for (auto &ext : prog_.externs()) {
        if (ext.getName() == "caml_alloc1") {
          EmitCamlAlloc(1);
          needsCallGC = true;
          continue;
        }
        if (ext.getName() == "caml_alloc2") {
          EmitCamlAlloc(2);
          needsCallGC = true;
          continue;
        }
        if (ext.getName() == "caml_alloc3") {
          EmitCamlAlloc(3);
          needsCallGC = true;
          continue;
        }
        if (ext.getName() == "caml_allocN") {
          EmitCamlAlloc({});
          needsCallGC = true;
          continue;
        }
      }
      if (needsCallGC) {
        EmitCamlCallGc();
      }
    }

    bool needsCCall = false;
    {
      for (auto &func : prog_) {
        if (func.GetCallingConv() != CallingConv::CAML) {
          for (auto *user : func.users()) {
            if (auto *movInst = ::cast_or_null<const MovInst>(user)) {
              auto *caller = movInst->getParent()->getParent();
              if (caller->GetCallingConv() == CallingConv::CAML) {
                needsCCall = true;
                break;
              }
            }
          }
        }
      }
    }
    if (needsCCall) {
      EmitCamlCCall();
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
