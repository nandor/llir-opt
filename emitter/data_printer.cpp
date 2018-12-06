// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/CodeGen/MachineModuleInfo.h>
#include <llvm/IR/Mangler.h>

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
    const llvm::MCObjectFileInfo *objInfo,
    const llvm::DataLayout &layout)
  : llvm::ModulePass(ID)
  , prog_(prog)
  , ctx_(ctx)
  , os_(os)
  , objInfo_(objInfo)
  , layout_(layout)
{
}

// -----------------------------------------------------------------------------
bool DataPrinter::runOnModule(llvm::Module &)
{
  if (auto *data = prog_->GetData()) {
    if (!data->IsEmpty()) {
      os_->SwitchSection(objInfo_->getDataSection());
      LowerSection(data);
    }
  }
  if (auto *cst = prog_->GetConst()) {
    if (!cst->IsEmpty()) {
      os_->SwitchSection(objInfo_->getDataSection());
      LowerSection(cst);
    }
  }
  if (auto *bss = prog_->GetBSS()) {
    if (!bss->IsEmpty()) {
      os_->SwitchSection(objInfo_->getDataBSSSection());
      LowerSection(bss);
    }
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
  for (auto &atom :  *data) {
    os_->EmitLabel(LowerSymbol(atom.GetName()));
    for (auto &item : atom) {
      switch (item->GetKind()) {
        case Item::Kind::INT8:  os_->EmitIntValue(item->GetInt8(),  1); break;
        case Item::Kind::INT16: os_->EmitIntValue(item->GetInt16(), 2); break;
        case Item::Kind::INT32: os_->EmitIntValue(item->GetInt32(), 4); break;
        case Item::Kind::INT64: os_->EmitIntValue(item->GetInt64(), 8); break;
        case Item::Kind::FLOAT64: {
          os_->EmitIntValue(item->GetFloat64(), 8);
          break;
        }
        case Item::Kind::SYMBOL: {
          os_->EmitSymbolValue(LowerSymbol(item->GetSymbol()->GetName()), 8);
          break;
        }
        case Item::Kind::ALIGN:  {
          os_->EmitValueToAlignment(1 << item->GetAlign());
          break;
        }
        case Item::Kind::SPACE:  {
          os_->EmitZeros(item->GetSpace());
          break;
        }
        case Item::Kind::STRING: {
          os_->EmitBytes(item->GetString());
          break;
        }
      }
    }
  }
}

// -----------------------------------------------------------------------------
llvm::MCSymbol *DataPrinter::LowerSymbol(const std::string_view name)
{
  llvm::SmallString<128> sym;
  llvm::Mangler::getNameWithPrefix(sym, name.data(), layout_);
  return ctx_->getOrCreateSymbol(sym);
}
