// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/CodeGen/MachineModuleInfo.h>
#include <llvm/IR/Mangler.h>

#include "core/data.h"
#include "core/prog.h"
#include "core/block.h"
#include "emitter/isel.h"
#include "emitter/data_printer.h"

using MCSymbol = llvm::MCSymbol;



// -----------------------------------------------------------------------------
char DataPrinter::ID;



// -----------------------------------------------------------------------------
DataPrinter::DataPrinter(
    const Prog *prog,
    ISel *isel,
    llvm::MCContext *ctx,
    llvm::MCStreamer *os,
    const llvm::MCObjectFileInfo *objInfo,
    const llvm::DataLayout &layout)
  : llvm::ModulePass(ID)
  , prog_(prog)
  , isel_(isel)
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
      os_->EmitLabel(ctx_->getOrCreateSymbol("_caml_data_begin"));
      LowerSection(bss);
      os_->EmitLabel(ctx_->getOrCreateSymbol("_caml_data_end"));
      os_->EmitIntValue(0, 8);
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
  AU.setPreservesAll();
  AU.addRequired<llvm::MachineModuleInfo>();
}

// -----------------------------------------------------------------------------
void DataPrinter::LowerSection(const Data *data)
{
  auto &moduleInfo = getAnalysis<llvm::MachineModuleInfo>();
  for (auto &atom :  *data) {
    auto *sym = LowerSymbol(atom.GetName());
    os_->EmitSymbolAttribute(sym, llvm::MCSA_Global);
    os_->EmitLabel(sym);
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
          auto *symbol = item->GetSymbol();
          switch (symbol->GetKind()) {
            case Global::Kind::BLOCK: {
              auto *block = static_cast<Block *>(symbol);
              auto *bb = (*isel_)[block]->getBasicBlock();
              auto *sym = moduleInfo.getAddrLabelSymbol(bb);
              os_->EmitSymbolValue(sym, 8);
              break;
            }
            case Global::Kind::SYMBOL:
            case Global::Kind::EXTERN:
            case Global::Kind::FUNC:
            case Global::Kind::ATOM: {
              os_->EmitSymbolValue(LowerSymbol(symbol->GetName()), 8);
              break;
            }
          }
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
