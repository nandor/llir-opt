// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/BinaryFormat/ELF.h>
#include <llvm/CodeGen/MachineModuleInfo.h>
#include <llvm/IR/Mangler.h>
#include <llvm/MC/MCSectionELF.h>

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
    const Prog &prog,
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
  for (const auto &data : prog_.data()) {
    if (data.IsEmpty()) {
      continue;
    }

    auto name = std::string(data.GetName());
    if (name == "caml") {
      // OCaml data section, simply the data section with end markers.
      auto emitValue = [&] (const std::string_view name) {
        os_->SwitchSection(GetCamlSection());
        auto *ptr = ctx_->createTempSymbol();
        os_->EmitLabel(ptr);

        os_->SwitchSection(GetDataSection());
        os_->EmitLabel(LowerSymbol(name));
        os_->EmitSymbolValue(ptr, 8);
      };

      emitValue("caml_data_begin");

      os_->SwitchSection(GetCamlSection());
      LowerSection(data);

      emitValue("caml_data_end");
      os_->EmitIntValue(0, 8);
    } else if (name == "data") {
      // Mutable data section.
      os_->SwitchSection(GetDataSection());
      LowerSection(data);
    } else if (name == "const") {
      // Immutable data section.
      os_->SwitchSection(GetConstSection());
      LowerSection(data);
    } else if (name == "bss") {
      // Zero-initialised mutable data section.
      os_->SwitchSection(GetBSSSection());
      LowerSection(data);
    } else {
      llvm::report_fatal_error("Unknown section '" + name + "'");
    }
  }

  return false;
}

// -----------------------------------------------------------------------------
llvm::StringRef DataPrinter::getPassName() const
{
  return "LLIR Data Section Printer";
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
    if (atom.GetAlignment() > 1) {
      os_->EmitValueToAlignment(atom.GetAlignment());
    }
    auto *sym = LowerSymbol(atom.GetName());
    os_->EmitLabel(sym);
    for (const Item &item : atom) {
      switch (item.GetKind()) {
        case Item::Kind::INT8:  os_->EmitIntValue(item.GetInt8(),  1); break;
        case Item::Kind::INT16: os_->EmitIntValue(item.GetInt16(), 2); break;
        case Item::Kind::INT32: os_->EmitIntValue(item.GetInt32(), 4); break;
        case Item::Kind::INT64: os_->EmitIntValue(item.GetInt64(), 8); break;
        case Item::Kind::FLOAT64: {
          os_->EmitIntValue(item.GetFloat64(), 8);
          break;
        }
        case Item::Kind::EXPR: {
          auto *expr = item.GetExpr();
          switch (expr->GetKind()) {
            case Expr::Kind::SYMBOL_OFFSET: {
              auto *offsetExpr = static_cast<SymbolOffsetExpr *>(expr);
              if (auto *symbol = offsetExpr->GetSymbol()) {
                MCSymbol *sym;
                switch (symbol->GetKind()) {
                  case Global::Kind::BLOCK: {
                    auto *block = static_cast<Block *>(symbol);
                    auto *bb = (*isel_)[block]->getBasicBlock();
                    sym = moduleInfo.getAddrLabelSymbol(bb);
                    break;
                  }
                  case Global::Kind::EXTERN:
                  case Global::Kind::FUNC:
                  case Global::Kind::ATOM: {
                    sym = LowerSymbol(symbol->GetName());
                    break;
                  }
                }
                if (auto offset = offsetExpr->GetOffset()) {
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
          }
          break;
        }
        case Item::Kind::ALIGN:  {
          os_->EmitValueToAlignment(item.GetAlign());
          break;
        }
        case Item::Kind::SPACE:  {
          os_->EmitZeros(item.GetSpace());
          break;
        }
        case Item::Kind::STRING: {
          os_->EmitBytes(item.GetString());
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

// -----------------------------------------------------------------------------
llvm::MCSection *DataPrinter::GetCamlSection()
{
  return GetDataSection();
}

// -----------------------------------------------------------------------------
llvm::MCSection *DataPrinter::GetDataSection()
{
  return objInfo_->getDataSection();
}

// -----------------------------------------------------------------------------
llvm::MCSection *DataPrinter::GetConstSection()
{
  switch (objInfo_->getObjectFileType()) {
    case llvm::MCObjectFileInfo::IsELF: {
      return ctx_->getELFSection(".rodata", llvm::ELF::SHT_PROGBITS, 0);
    }
    case llvm::MCObjectFileInfo::IsMachO: {
      return objInfo_->getConstDataSection();
    }
    case llvm::MCObjectFileInfo::IsCOFF: {
      llvm_unreachable("Unsupported output: COFF");
    }
    case llvm::MCObjectFileInfo::IsWasm: {
      llvm_unreachable("Unsupported output: Wasm");
    }
    case llvm::MCObjectFileInfo::IsLLIR: {
      llvm_unreachable("Unsupported output: LLIR");
    }
  }
  llvm_unreachable("invalid section kind");
}

// -----------------------------------------------------------------------------
llvm::MCSection *DataPrinter::GetBSSSection()
{
  switch (objInfo_->getObjectFileType()) {
    case llvm::MCObjectFileInfo::IsELF: {
      return ctx_->getELFSection(".bss", llvm::ELF::SHT_NOBITS, 0);
    }
    case llvm::MCObjectFileInfo::IsMachO: {
      return objInfo_->getConstDataSection();
    }
    case llvm::MCObjectFileInfo::IsCOFF: {
      llvm_unreachable("Unsupported output: COFF");
    }
    case llvm::MCObjectFileInfo::IsWasm: {
      llvm_unreachable("Unsupported output: Wasm");
    }
    case llvm::MCObjectFileInfo::IsLLIR: {
      llvm_unreachable("Unsupported output: LLIR");
    }
  }
  llvm_unreachable("invalid section kind");
}

