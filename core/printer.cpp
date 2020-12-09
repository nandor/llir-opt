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
    os_ << "\t.extern\t" << ext.getName() << ", ";
    Print(ext.GetVisibility());
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
  os_ << "\t.visibility\t"; Print(atom.GetVisibility()); os_ << "\n";
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
      case Item::Kind::ALIGN: {
        os_ << "\t.align\t" << item.GetAlign();
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
  os_ << "\t.visibility\t"; Print(func.GetVisibility()); os_ << "\n";
  os_ << "\t.call\t"; Print(func.GetCallingConv()); os_ << "\n";
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
      Print(params[i]);
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
  os_ << "\t.visibility\t"; Print(block.GetVisibility()); os_ << "\n";
  for (const Inst &i : block) {
    Print(i);
  }
}

// -----------------------------------------------------------------------------
static const char *kNames[] =
{
  #define GET_INST(kind, type, name, sort) name,
  #include "instructions.def"
};

// -----------------------------------------------------------------------------
void Printer::Print(const Inst &inst)
{
  os_ << "\t" << kNames[static_cast<uint8_t>(inst.GetKind())];
  // Print instruction-specific attributes.
  switch (inst.GetKind()) {
    case Inst::Kind::INVOKE:
    case Inst::Kind::CALL: {
      auto &call = static_cast<const CallSite &>(inst);
      if (auto size = call.GetNumFixedArgs()) {
        os_ << "." << *size;
      }
      os_ << ".";
      Print(call.GetCallingConv());
      break;
    }
    case Inst::Kind::TAIL_CALL: {
      auto &call = static_cast<const CallSite &>(inst);
      if (auto size = call.GetNumFixedArgs()) {
        os_ << "." << *size;
      }
      os_ << ".";
      Print(call.GetCallingConv());
      for (const Type type : call.types()) {
        os_ << "."; Print(type);
      }
      break;
    }
    case Inst::Kind::RAISE: {
      auto &raise = static_cast<const RaiseInst &>(inst);
      if (auto cc = raise.GetCallingConv()) {
        os_ << ".";
        Print(*cc);
      }
      break;
    }
    case Inst::Kind::CMP: {
      os_ << ".";
      Print(static_cast<const CmpInst &>(inst).GetCC());
      break;
    }
    default: {
      break;
    }
  }
  // Print the return value (if it exists).
  if (auto numRet = inst.GetNumRets()) {
    for (unsigned i = 0; i < numRet; ++i) {
      os_ << ".";
      Print(inst.GetType(i));
    }
    os_ << "\t";
    for (unsigned i = 0; i < numRet; ++i) {
      if (i != 0) {
        os_ << ", ";
      }
      auto it = insts_.find(ConstRef(&inst, i));
      if (it == insts_.end()) {
        os_ << "$<" << &inst << ">";
      } else {
        os_ << "$" << it->second;
      }
    }
  } else {
    os_ << "\t";
  }
  // Print the arguments.
  for (auto it = inst.value_op_begin(); it != inst.value_op_end(); ++it) {
    if (inst.GetNumRets() || it != inst.value_op_begin()) {
      os_ << ", ";
    }
    Print(*it);
  }
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
          switch (static_cast<const ConstantReg &>(c).GetValue()) {
            case ConstantReg::Kind::SP:           os_ << "$sp";           return;
            case ConstantReg::Kind::FS:           os_ << "$fs";           return;
            case ConstantReg::Kind::RET_ADDR:     os_ << "$ret_addr";     return;
            case ConstantReg::Kind::FRAME_ADDR:   os_ << "$frame_addr";   return;
            case ConstantReg::Kind::AARCH64_FPSR: os_ << "$aarch64_fpsr"; return;
            case ConstantReg::Kind::AARCH64_FPCR: os_ << "$aarch64_fpcr"; return;
            case ConstantReg::Kind::RISCV_FFLAGS: os_ << "$riscv_fflags"; return;
            case ConstantReg::Kind::RISCV_FRM:    os_ << "$riscv_frm";    return;
            case ConstantReg::Kind::RISCV_FCSR:   os_ << "$riscv_fcsr";   return;
            case ConstantReg::Kind::PPC_FPSCR:    os_ << "$ppc_fpscr";    return;
          }
          llvm_unreachable("invalid register kind");
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
void Printer::Print(Type type)
{
  os_ << type;
}

// -----------------------------------------------------------------------------
void Printer::Print(TypeFlag flag)
{
  switch (flag.GetKind()) {
    case TypeFlag::Kind::NONE: {
      return;
    }
    case TypeFlag::Kind::SEXT: {
      os_ << ":sext";
      return;
    }
    case TypeFlag::Kind::ZEXT: {
      os_ << ":zext";
      return;
    }
    case TypeFlag::Kind::BYVAL: {
      os_ << ":byval";
      os_ << ":" << flag.GetByValSize();
      os_ << ":" << flag.GetByValAlign().value();
      return;
    }
  }
  llvm_unreachable("invalid type flag");
}

// -----------------------------------------------------------------------------
void Printer::Print(FlaggedType type)
{
  Print(type.GetType());
  Print(type.GetFlag());
}

// -----------------------------------------------------------------------------
void Printer::Print(CallingConv conv)
{
  switch (conv) {
    case CallingConv::C:          os_ << "c";          return;
    case CallingConv::CAML:       os_ << "caml";       return;
    case CallingConv::CAML_ALLOC: os_ << "caml_alloc"; return;
    case CallingConv::CAML_GC:    os_ << "caml_gc";    return;
    case CallingConv::SETJMP:     os_ << "setjmp";     return;
    case CallingConv::XEN:        os_ << "xen";        return;
  }
  llvm_unreachable("invalid calling convention");
}

// -----------------------------------------------------------------------------
void Printer::Print(Cond cc)
{
  switch (cc) {
    case Cond::EQ:  os_ << "eq";  return;
    case Cond::OEQ: os_ << "oeq"; return;
    case Cond::UEQ: os_ << "ueq"; return;
    case Cond::NE:  os_ << "ne";  return;
    case Cond::ONE: os_ << "one"; return;
    case Cond::UNE: os_ << "une"; return;
    case Cond::LT:  os_ << "lt";  return;
    case Cond::OLT: os_ << "olt"; return;
    case Cond::ULT: os_ << "ult"; return;
    case Cond::GT:  os_ << "gt";  return;
    case Cond::OGT: os_ << "ogt"; return;
    case Cond::UGT: os_ << "ugt"; return;
    case Cond::LE:  os_ << "le";  return;
    case Cond::OLE: os_ << "ole"; return;
    case Cond::ULE: os_ << "ule"; return;
    case Cond::GE:  os_ << "ge";  return;
    case Cond::OGE: os_ << "oge"; return;
    case Cond::UGE: os_ << "uge"; return;
    case Cond::O:   os_ << "o";   return;
    case Cond::UO:  os_ << "uo";  return;
  }
  llvm_unreachable("invalid condition code");
}

// -----------------------------------------------------------------------------
void Printer::Print(Visibility visibility)
{
  switch (visibility) {
    case Visibility::LOCAL:           os_ << "local";           return;
    case Visibility::GLOBAL_DEFAULT:  os_ << "global_default";  return;
    case Visibility::GLOBAL_HIDDEN:   os_ << "global_hidden";   return;
    case Visibility::WEAK_DEFAULT:    os_ << "weak_default";    return;
    case Visibility::WEAK_HIDDEN:     os_ << "weak_hidden";     return;
  }
  llvm_unreachable("invalid visibility");
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
