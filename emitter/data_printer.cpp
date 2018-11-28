// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/CodeGen/MachineModuleInfo.h>

#include "core/data.h"
#include "core/prog.h"
#include "emitter/data_printer.h"

using MCSymbol = llvm::MCSymbol;



// -----------------------------------------------------------------------------
char DataPrinter::ID;



// -----------------------------------------------------------------------------
DataPrinter::DataPrinter(
    const Prog *prog,
    llvm::MCContext *ctx,
    llvm::MCStreamer *os,
    const llvm::MCObjectFileInfo *objInfo)
  : llvm::ModulePass(ID)
  , prog_(prog)
  , ctx_(ctx)
  , os_(os)
  , objInfo_(objInfo)
{
}

// -----------------------------------------------------------------------------
bool DataPrinter::runOnModule(llvm::Module &)
{
  if (auto *data = prog_->GetData()) {
    os_->SwitchSection(objInfo_->getDataSection());
    LowerSection(data);
  }
  if (auto *cst = prog_->GetConst()) {
    os_->SwitchSection(objInfo_->getDataSection());
    LowerSection(cst);
  }
  if (auto *bss = prog_->GetBSS()) {
    os_->SwitchSection(objInfo_->getDataBSSSection());
    LowerSection(bss);
  }
  return false;
}

// -----------------------------------------------------------------------------
llvm::StringRef DataPrinter::getPassName() const
{
  return "GenM Data Section Printer";
}

// -----------------------------------------------------------------------------
void DataPrinter::getAnalysisUsage(llvm::AnalysisUsage &AU) const
{
  AU.addRequired<llvm::MachineModuleInfo>();
  AU.addPreserved<llvm::MachineModuleInfo>();
}

// -----------------------------------------------------------------------------
void DataPrinter::LowerSection(const Data *data)
{
  llvm::errs() << "Section!\n";
  for (auto &atom :  *data) {
    if (Symbol *gmSym = atom.GetSymbol()) {
      llvm::errs() << gmSym->GetName().data() << "\n";
      os_->EmitLabel(ctx_->getOrCreateSymbol(gmSym->GetName().data()));
    }
  }
}
