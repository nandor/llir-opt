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
#include "emitter/isel_mapping.h"
#include "emitter/data_printer.h"

using MCSymbol = llvm::MCSymbol;



// -----------------------------------------------------------------------------
char DataPrinter::ID;



// -----------------------------------------------------------------------------
DataPrinter::DataPrinter(
    const Prog &prog,
    ISelMapping *isel,
    llvm::MCContext *ctx,
    llvm::MCStreamer *os,
    const llvm::MCObjectFileInfo *objInfo,
    const llvm::DataLayout &layout,
    bool shared)
  : llvm::ModulePass(ID)
  , prog_(prog)
  , isel_(isel)
  , ctx_(ctx)
  , os_(os)
  , objInfo_(objInfo)
  , layout_(layout)
  , shared_(shared)
{
}

// -----------------------------------------------------------------------------
bool DataPrinter::runOnModule(llvm::Module &)
{
  for (const Extern &ext : prog_.externs()) {
    auto *sym = LowerSymbol(ext.getName());
    if (auto *alias = ext.GetAlias()) {
      os_->emitAssignment(
          sym,
          llvm::MCSymbolRefExpr::create(LowerSymbol(alias->getName()), *ctx_)
      );
    }
    EmitVisibility(sym, ext.GetVisibility());
  }

  for (const auto &data : prog_.data()) {
    if (data.IsEmpty()) {
      continue;
    }

    auto name = std::string(data.GetName());
    if (name == ".data.caml") {
      // OCaml data section, simply the data section with end markers.
      auto emitValue = [&] (const std::string_view name) {
        auto *prefix = shared_ ? "caml_shared_startup__data" : "caml__data";
        llvm::SmallString<128> mangledName;
        llvm::Mangler::getNameWithPrefix(
            mangledName,
            std::string(prefix) + std::string(name),
            layout_
        );

        os_->SwitchSection(GetCamlSection());
        auto *sym = ctx_->getOrCreateSymbol(mangledName);
        if (shared_) {
          os_->emitSymbolAttribute(sym, llvm::MCSA_Global);
        }
        os_->emitLabel(sym);
      };

      os_->SwitchSection(GetCamlSection());
      emitValue("_begin");
      LowerSection(data);
      emitValue("_end");
      os_->emitIntValue(0, 8);
      continue;
    }

    if (name.substr(0, 5) == ".data") {
      // Mutable data section.
      os_->SwitchSection(GetDataSection());
      LowerSection(data);
      continue;
    }
    if (name.substr(0, 7) == ".const") {
      // Immutable data section.
      os_->SwitchSection(GetConstSection());
      LowerSection(data);
      continue;
    }
    if (name.substr(0, 4) == ".bss") {
      // Zero-initialised mutable data section.
      os_->SwitchSection(GetBSSSection());
      LowerSection(data);
      continue;
    }
    llvm::report_fatal_error("Unknown section '" + name + "'");
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
  AU.addRequired<llvm::MachineModuleInfoWrapperPass>();
}

// -----------------------------------------------------------------------------
void DataPrinter::LowerSection(const Data &data)
{
  for (const Object &object : data) {
    LowerObject(object);
  }
}

// -----------------------------------------------------------------------------
void DataPrinter::LowerObject(const Object &object)
{
  for (const Atom &atom : object) {
    LowerAtom(atom);
  }
}

// -----------------------------------------------------------------------------
void DataPrinter::LowerAtom(const Atom &atom)
{
  auto &moduleInfo = getAnalysis<llvm::MachineModuleInfoWrapperPass>().getMMI();
  if (atom.GetAlignment() > 1) {
    os_->emitValueToAlignment(atom.GetAlignment().value());
  }
  auto *sym = LowerSymbol(atom.GetName());
  EmitVisibility(sym, atom.GetVisibility());
  os_->emitSymbolAttribute(sym, llvm::MCSA_ELF_TypeObject);
  os_->emitLabel(sym);
  for (const Item &item : atom) {
    switch (item.GetKind()) {
      case Item::Kind::INT8:  os_->emitIntValue(item.GetInt8(),  1); break;
      case Item::Kind::INT16: os_->emitIntValue(item.GetInt16(), 2); break;
      case Item::Kind::INT32: os_->emitIntValue(item.GetInt32(), 4); break;
      case Item::Kind::INT64: os_->emitIntValue(item.GetInt64(), 8); break;
      case Item::Kind::FLOAT64: {
        union U { double f; uint64_t i; } u = { .f = item.GetFloat64() };
        os_->emitIntValue(u.i, 8);
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
                os_->emitValue(
                    llvm::MCBinaryExpr::createAdd(
                        llvm::MCSymbolRefExpr::create(sym, *ctx_),
                        llvm::MCConstantExpr::create(offset, *ctx_),
                        *ctx_
                    ),
                    8
                );
              } else {
                os_->emitSymbolValue(sym, 8);
              }
            } else {
              os_->emitIntValue(0ull, 8);
            }
            break;
          }
        }
        break;
      }
      case Item::Kind::ALIGN:  {
        os_->emitValueToAlignment(item.GetAlign());
        break;
      }
      case Item::Kind::SPACE:  {
        os_->emitZeros(item.GetSpace());
        break;
      }
      case Item::Kind::STRING: {
        os_->emitBytes(item.getString());
        break;
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
void DataPrinter::EmitVisibility(llvm::MCSymbol *sym, Visibility visibility)
{
  switch (visibility) {
    case Visibility::LOCAL: {
      return;
    }
    case Visibility::GLOBAL_DEFAULT: {
      os_->emitSymbolAttribute(sym, llvm::MCSA_Global);
      return;
    }
    case Visibility::GLOBAL_HIDDEN: {
      os_->emitSymbolAttribute(sym, llvm::MCSA_Global);
      os_->emitSymbolAttribute(sym, llvm::MCSA_Hidden);
      return;
    }
    case Visibility::WEAK_DEFAULT: {
      os_->emitSymbolAttribute(sym, llvm::MCSA_Weak);
      return;
    }
    case Visibility::WEAK_HIDDEN: {
      os_->emitSymbolAttribute(sym, llvm::MCSA_Weak);
      os_->emitSymbolAttribute(sym, llvm::MCSA_Hidden);
      return;
    }
  }
  llvm_unreachable("invalid visibility attribute");
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
    case llvm::MCObjectFileInfo::IsXCOFF: {
      llvm_unreachable("Unsupported output: XCOFF");
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
    case llvm::MCObjectFileInfo::IsXCOFF: {
      llvm_unreachable("Unsupported output: XCOFF");
    }
  }
  llvm_unreachable("invalid section kind");
}
