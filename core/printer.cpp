// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/Support/Format.h>

#include "core/printer.h"
#include "core/block.h"
#include "core/cfg.h"
#include "core/constant.h"
#include "core/data.h"
#include "core/expr.h"
#include "core/func.h"
#include "core/insts.h"
#include "core/prog.h"
#include "core/extern.h"



// -----------------------------------------------------------------------------
void Printer::Print(const Prog &prog)
{
  // Print the module name.
  os_ << "\t.file \"" << prog.getName() << "\"\n";

  // Print aliases and externs.
  for (const Extern &ext : prog.externs()) {
    os_ << "\t.extern\t" << ext.getName() << ", " << ext.GetVisibility();
    if (auto *g = ext.GetAlias()) {
      os_ << ", " << g->getName();
    }
    os_ << "\n";
  }

  // Print the text segment.
  os_ << "\t.section .text\n";
  for (const Func &f : prog) {
    Print(f);
  }
  os_ << "\n";

  // Print all data segments.
  for (const Data &data : prog.data()) {
    os_ << "\t.section\t" << data.getName() << "\n";
    Print(data);
    os_ << "\n";
  }

  // Print all xtors.
  for (const Xtor &xtor : prog.xtor()) {
    switch (xtor.getKind()) {
      case Xtor::Kind::CTOR: {
        os_ << "\t.ctor ";
        break;
      }
      case Xtor::Kind::DTOR: {
        os_ << "\t.dtor ";
        break;
      }
    }
    os_ << xtor.getPriority() << ", " << xtor.getFunc()->getName() << "\n";
  }
}

// -----------------------------------------------------------------------------
void Printer::Print(const Data &data)
{
  for (auto &object : data) {
    Print(object);
  }
}

// -----------------------------------------------------------------------------
void Printer::Print(const Object &object)
{
  for (auto &atom : object) {
    Print(atom);
  }
  os_ << "\t.end\n";
}

// -----------------------------------------------------------------------------
void Printer::Print(const Atom &atom)
{
  if (auto align = atom.GetAlignment()) {
    os_ << "\t.align\t" << align->value() << "\n";
  }
  os_ << atom.getName() << ":\n";
  os_ << "\t.visibility\t" << atom.GetVisibility() << "\n";
  for (auto &item : atom) {
    switch (item.GetKind()) {
      case Item::Kind::INT8: {
        os_ << "\t.byte\t"   << item.GetInt8();
        break;
      }
      case Item::Kind::INT16: {
        os_ << "\t.short\t"  << item.GetInt16();
        break;
      }
      case Item::Kind::INT32: {
        os_ << "\t.long\t"   << item.GetInt32();
        break;
      }
      case Item::Kind::INT64: {
        os_ << "\t.quad\t"   << item.GetInt64();
        break;
      }
      case Item::Kind::FLOAT64: {
        os_ << "\t.double\t" << item.GetFloat64();
        break;
      }
      case Item::Kind::EXPR: {
        auto *expr = item.GetExpr();
        switch (expr->GetKind()) {
          case Expr::Kind::SYMBOL_OFFSET: {
            auto *offsetExpr = static_cast<SymbolOffsetExpr *>(expr);
            if (auto *symbol = offsetExpr->GetSymbol()) {
              os_ << "\t.quad\t" << symbol->getName();
              if (auto offset = offsetExpr->GetOffset()) {
                if (offset < 0) {
                  os_ << "-" << -offset;
                }
                if (offset > 0) {
                  os_ << "+" << +offset;
                }
              }
            } else {
              os_ << "\t.quad\t" << 0ull;
            }
            break;
          }
        }
        break;
      }
      case Item::Kind::SPACE: {
        os_ << "\t.space\t" << item.GetSpace();
        break;
      }
      case Item::Kind::STRING: {
        os_ << "\t.ascii\t";
        Print(item.GetString());
        break;
      }
    }
    os_ << "\n";
  }
  os_ << "\n";
}

// -----------------------------------------------------------------------------
void Printer::Print(const Func &func)
{
  if (auto align = func.GetAlignment()) {
    os_ << "\t.align\t" << align->value() << "\n";
  }
  os_ << func.getName() << ":\n";
  // Print attributes.
  os_ << "\t.visibility\t" << func.GetVisibility() << "\n";
  os_ << "\t.call\t" << func.GetCallingConv() << "\n";
  if (func.IsNoInline()) {
    os_ << "\t.noinline\n";
  }
  if (func.IsVarArg()) {
    os_ << "\t.vararg\n";
  }
  if (auto features = func.getFeatures(); !features.empty()) {
    os_ << "\t.features\t\"" << features << "\"\n";
  }

  // Print stack objects.
  for (auto &o : func.objects()) {
    os_ << "\t.stack_object\t";
    os_ << o.Index << ", " << o.Size << ", " << o.Alignment.value();
    os_ << "\n";
  }

  // Print argument types.
  {
    os_ << "\t.args\t";
    auto params = func.params();
    for (unsigned i = 0; i < params.size(); ++i) {
      if (i != 0) {
        os_ << ", ";
      }
      os_ << params[i];
    }
    os_ << "\n";
  }

  // Generate names for instructions.
  {
    llvm::ReversePostOrderTraversal<const Func *> rpot(&func);
    for (const Block &block : func) {
      for (const Inst &inst : block) {
        for (unsigned i = 0, n = inst.GetNumRets(); i < n; ++i) {
          insts_.emplace(ConstRef(&inst, i), insts_.size());
        }
      }
    }
    for (const Block &block : func) {
      Print(block);
    }
  }

  insts_.clear();
  os_ << "\t.end\n\n";
}

// -----------------------------------------------------------------------------
void Printer::Print(const Block &block)
{
  os_ << block.getName() << ":\n";
  os_ << "\t.visibility\t" << block.GetVisibility() << "\n";
  for (const Inst &i : block) {
    Print(i);
  }
}
// -----------------------------------------------------------------------------
void Printer::Print(const Inst &inst)
{
  os_ << "\t";

  // Print the main instruction.
  PrintImpl(inst);

  // Print any annotations.
  for (const auto &annot : inst.annots()) {
    os_ << " ";
    switch (annot.GetKind()) {
      case Annot::Kind::CAML_FRAME: {
        auto &frame = static_cast<const CamlFrame &>(annot);
        os_ << "@caml_frame((";
        {
          bool first = true;
          for (const auto &size : frame.allocs()) {
            if (!first) {
              os_ << " ";
            }
            first = false;
            os_ << size;
          }
        }
        os_ << ") (";
        {
          for (const auto &debug_info : frame.debug_infos()) {
            os_ << "(";
            for (const auto &debug : debug_info) {
              os_ << "(" << debug.Location << " ";
              Print(debug.File);
              os_ << " ";
              Print(debug.Definition);
              os_ << ")";
            }
            os_ << ")";
          }
        }
        os_ << "))";
        break;
      }
      case Annot::Kind::PROBABILITY: {
        auto &p = static_cast<const Probability &>(annot);
        uint32_t n = p.GetNumerator();
        uint32_t d = p.GetDenumerator();
        os_ << "@probability(" << n << " " << d << ")";
        break;
      }
    }
  }
  os_ << "\n";
}

// -----------------------------------------------------------------------------
void Printer::Print(ConstRef<Value> val)
{
  if (reinterpret_cast<uintptr_t>(val.Get()) & 1) {
    os_ << "<" << (reinterpret_cast<uintptr_t>(val.Get()) >> 1) << ">";
    return;
  }

  switch (val->GetKind()) {
    case Value::Kind::INST: {
      auto it = insts_.find(cast<Inst>(val));
      if (it == insts_.end()) {
        os_ << "$<" << val.Get() << ":" << val.Index() << ">";
      } else {
        os_ << "$" << it->second;
      }
      return;
    }
    case Value::Kind::GLOBAL: {
      os_ << cast<Global>(val)->getName();
      return;
    }
    case Value::Kind::EXPR: {
      Print(*cast<Expr>(val));
      return;
    }
    case Value::Kind::CONST: {
      const Constant &c = *cast<Constant>(val);
      switch (c.GetKind()) {
        case Constant::Kind::INT: {
          os_ << static_cast<const ConstantInt &>(c).GetValue();
          return;
        }
        case Constant::Kind::FLOAT: {
          union { double d; int64_t i; };
          d = static_cast<const ConstantFloat &>(c).GetDouble();
          os_ << llvm::format("0x%016" PRIx64, i);
          return;
        }
        case Constant::Kind::REG: {
          os_ << static_cast<const ConstantReg &>(c).GetValue();
          return;
        }
      }
      llvm_unreachable("invalid constant kind");
    }
  }
  llvm_unreachable("invalid value kind");
}

// -----------------------------------------------------------------------------
void Printer::Print(const Expr &expr)
{
  switch (expr.GetKind()) {
    case Expr::Kind::SYMBOL_OFFSET: {
      auto &symExpr = static_cast<const SymbolOffsetExpr &>(expr);
      os_ << symExpr.GetSymbol()->getName();
      int64_t off = symExpr.GetOffset();
      if (off < 0) {
        os_ << " - " << -off;
      } else {
        os_ << " + " << +off;
      }
      break;
    }
  }
}

// -----------------------------------------------------------------------------
void Printer::Print(const std::string_view str)
{
  os_ << "\"";
  for (const uint8_t c : str) {
    switch (c) {
      case '\t': os_ << "\\t"; break;
      case '\n': os_ << "\\n"; break;
      case '\\': os_ << "\\\\"; break;
      case '\"': os_ << "\\\""; break;
      default: {
        if (isprint(c)) {
          os_ << c;
        } else {
          os_ << "\\";
          os_ << static_cast<char>('0' + ((c / 8 / 8) % 8));
          os_ << static_cast<char>('0' + ((c / 8) % 8));
          os_ << static_cast<char>('0' + (c % 8));
        }
      }
      break;
    }
  }
  os_ << "\"";
}

// -----------------------------------------------------------------------------
void Printer::PrintImpl(const Inst &i)
{
  switch (i.GetKind()) {
    #define GET_PRINTER
    #include "instructions.def"
    case Inst::Kind::PHI: {
      auto &phi = static_cast<const PhiInst &>(i);
      os_ << "phi\t" << phi.GetType() << ":"; Print(i.GetSubValue(0));
      for (unsigned i = 0, n = phi.GetNumIncoming(); i < n; ++i) {
        os_ << ", "; Print(phi.GetBlock(i));
        os_ << ", "; Print(phi.GetValue(i));
      }
      return;
    }
  }
  llvm_unreachable("not implemented");
}
