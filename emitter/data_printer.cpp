// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/CodeGen/MachineModuleInfo.h>

#include "core/data.h"
#include "core/prog.h"
#include "emitter/data_printer.h"



// -----------------------------------------------------------------------------
char DataPrinter::ID;

// -----------------------------------------------------------------------------
DataPrinter::DataPrinter(
    const Prog *prog,
    llvm::MCStreamer *os,
    const llvm::MCObjectFileInfo *objInfo)
  : llvm::ModulePass(ID)
  , prog_(prog)
  , os_(os)
  , objInfo_(objInfo)
{
}

// -----------------------------------------------------------------------------
bool DataPrinter::runOnModule(llvm::Module &)
{
  if (auto *data = prog_->GetData()) {
    LowerSection(data);
  }
  if (auto *bss = prog_->GetData()) {
    LowerSection(bss);
  }
  if (auto *cst = prog_->GetData()) {
    LowerSection(cst);
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

}
