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
  auto emitValue = [&] (const std::string_view name) {
    os_->SwitchSection(objInfo_->getDataSection());
    auto *ptr = ctx_->createTempSymbol();
    os_->EmitLabel(ptr);

    os_->SwitchSection(objInfo_->getDataSection());
    os_->EmitLabel(ctx_->getOrCreateSymbol(name.data()));
    os_->EmitSymbolValue(ptr, 8);
  };

  for (const auto &data : prog_->data()) {
    if (data.IsEmpty()) {
      continue;
    }

    auto name = std::string(data.GetName());
    if (name == "caml") {
      emitValue("_caml_data_begin");
      LowerSection(data);
      emitValue("_caml_data_end");
      os_->EmitIntValue(0, 8);
    } else if (name == "data") {
      os_->SwitchSection(objInfo_->getDataSection());
      LowerSection(data);
    } else if (name == "const") {
      os_->SwitchSection(objInfo_->getReadOnlySection());
      LowerSection(data);
    } else if (name == "bss") {
      os_->SwitchSection(objInfo_->getDataBSSSection());
      LowerSection(data);
    } else {
      throw std::runtime_error("Unknown section '" + name + "'");
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
void DataPrinter::LowerSection(const Data &data)
{
  auto &moduleInfo = getAnalysis<llvm::MachineModuleInfo>();
  for (auto &atom :  data) {
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
          if (auto *symbol = item->GetSymbol()) {
            MCSymbol *sym;
            switch (symbol->GetKind()) {
              case Global::Kind::BLOCK: {
                auto *block = static_cast<Block *>(symbol);
                auto *bb = (*isel_)[block]->getBasicBlock();
                sym = moduleInfo.getAddrLabelSymbol(bb);
                break;
              }
              case Global::Kind::SYMBOL:
              case Global::Kind::EXTERN:
              case Global::Kind::FUNC:
              case Global::Kind::ATOM: {
                sym = LowerSymbol(symbol->GetName());
                break;
              }
            }
            if (auto offset = item->GetOffset()) {
              os_->EmitValue(
                  llvm::MCBinaryExpr::createAdd(
                      llvm::MCSymbolRefExpr::create(sym, *ctx_),
                      llvm::MCConstantExpr::create(offset, *ctx_),
                      *ctx_
                  ),
                  8
              );
            } else {
              os_->EmitSymbolValue(sym, 8);
            }
          } else {
            os_->EmitIntValue(0ull, 8);
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
        case Item::Kind::END: {
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
