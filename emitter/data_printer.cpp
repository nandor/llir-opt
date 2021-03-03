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
    if (auto value = ext.GetValue()) {
      switch (value->GetKind()) {
        case Value::Kind::GLOBAL: {
          auto g = ::cast<Global>(value);
          os_->emitAssignment(
              sym,
              llvm::MCSymbolRefExpr::create(LowerSymbol(g->getName()), *ctx_)
          );
          break;
        }
        case Value::Kind::CONST: {
          switch (::cast<Constant>(value)->GetKind()) {
            case Constant::Kind::INT: {
              llvm_unreachable("not implemented");
            }
            case Constant::Kind::FLOAT: {
              llvm_unreachable("not implemented");
            }
          }
          break;
        }
        case Value::Kind::INST:
        case Value::Kind::EXPR: {
          llvm_unreachable("invalid alias");
        }
      }
    }
    EmitVisibility(sym, ext.GetVisibility());
  }

  for (const auto &data : prog_.data()) {
    if (data.IsEmpty()) {
      continue;
    }

    auto name = std::string(data.GetName());
    os_->SwitchSection(GetSection(data));
    if (name == ".data.caml") {
      // OCaml data section, simply the data section with end markers.
      auto emitMarker = [&] (const std::string_view name) {
        auto *prefix = shared_ ? "caml_shared_startup__data" : "caml__data";
        llvm::SmallString<128> mangledName;
        llvm::Mangler::getNameWithPrefix(
            mangledName,
            std::string(prefix) + std::string(name),
            layout_
        );
        auto *sym = ctx_->getOrCreateSymbol(mangledName);
        if (shared_) {
          os_->emitSymbolAttribute(sym, llvm::MCSA_Global);
        }
        os_->emitLabel(sym);
      };

      emitMarker("_begin");
      LowerSection(data);
      emitMarker("_end");
      os_->emitIntValue(0, 8);
    } else {
      LowerSection(data);
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
  if (auto align = atom.GetAlignment()) {
    os_->emitValueToAlignment(align->value());
  }
  auto *sym = LowerSymbol(atom.GetName());
  EmitVisibility(sym, atom.GetVisibility());
  os_->emitSymbolAttribute(sym, llvm::MCSA_ELF_TypeObject);
  os_->emitLabel(sym);
  for (const Item &item : atom) {
    switch (item.GetKind()) {
      case Item::Kind::INT8:  os_->emitIntValue(item.GetInt8(),  1); continue;
      case Item::Kind::INT16: os_->emitIntValue(item.GetInt16(), 2); continue;
      case Item::Kind::INT32: os_->emitIntValue(item.GetInt32(), 4); continue;
      case Item::Kind::INT64: os_->emitIntValue(item.GetInt64(), 8); continue;
      case Item::Kind::FLOAT64: {
        union U { double f; uint64_t i; } u = { .f = item.GetFloat64() };
        os_->emitIntValue(u.i, 8);
        continue;
      }
      case Item::Kind::EXPR32:
      case Item::Kind::EXPR64: {
        auto *expr = item.GetExpr();
        switch (expr->GetKind()) {
          case Expr::Kind::SYMBOL_OFFSET: {
            auto *offsetExpr = static_cast<const SymbolOffsetExpr *>(expr);
            if (auto *symbol = offsetExpr->GetSymbol()) {
              MCSymbol *sym;
              switch (symbol->GetKind()) {
                case Global::Kind::BLOCK: {
                  auto *block = static_cast<const Block *>(symbol);
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
                    item.GetSize()
                );
              } else {
                os_->emitSymbolValue(sym, item.GetSize());
              }
            } else {
              os_->emitIntValue(0ull, item.GetSize());
            }
            continue;
          }
        }
        llvm_unreachable("invalid expression kind");
      }
      case Item::Kind::SPACE:  {
        os_->emitZeros(item.GetSpace());
        continue;
      }
      case Item::Kind::STRING: {
        os_->emitBytes(item.getString());
        continue;
      }
    }
    llvm_unreachable("invalid item kind");
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
llvm::MCSection *DataPrinter::GetSection(const Data &data)
{
  switch (objInfo_->getObjectFileType()) {
    case llvm::MCObjectFileInfo::IsELF: {
      unsigned type;
      if (data.IsZeroed()) {
        type = llvm::ELF::SHT_NOBITS;
      } else {
        type = llvm::ELF::SHT_PROGBITS;
      }
      unsigned flags = llvm::ELF::SHF_ALLOC;
      if (data.IsWritable()) {
        flags |= llvm::ELF::SHF_WRITE;
      }
      return ctx_->getELFSection(data.getName(), type, flags);
    }
    case llvm::MCObjectFileInfo::IsMachO: {
      llvm_unreachable("Unsupported output: MachO");
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
