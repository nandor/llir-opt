// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/PostOrderIterator.h>

#include "core/printer.h"
#include "core/block.h"
#include "core/cfg.h"
#include "core/constant.h"
#include "core/data.h"
#include "core/expr.h"
#include "core/func.h"
#include "core/insts/binary.h"
#include "core/insts/call.h"
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
  os_ << "\t.align\t" << atom.GetAlignment().value() << "\n";
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
        os_ << ",";
      }
      Print(params[i]);
    }
    os_ << "\n";
  }

  // Generate names for instructions.
  {
    llvm::ReversePostOrderTraversal<const Func *> rpot(&func);
    for (const Block *block : rpot) {
      for (const Inst &inst : *block) {
        for (unsigned i = 0, n = inst.GetNumRets(); i < n; ++i) {
          insts_.emplace(ConstRef(&inst, i), insts_.size());
        }
      }
    }
    for (const Block *block : rpot) {
      Print(*block);
    }
  }

  insts_.clear();
  os_ << "\n";
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
  "call", "tcall", "invoke", "ret",
  "jcc", "jmp", "switch", "trap",
  "raise",
  "ld", "st",
  "vastart",
  "alloca",
  "arg", "frame", "undef",
  "select",
  "abs", "neg",  "sqrt", "sin", "cos",
  "sext", "zext", "fext", "xext",
  "mov", "trunc",
  "exp", "exp2", "log", "log2", "log10",
  "fceil", "ffloor",
  "popcnt",
  "clz", "ctz",
  "add", "and", "cmp",
  "udiv", "urem",
  "sdiv", "srem",
  "mul", "or",
  "rotl", "rotr",
  "sll", "sra", "srl", "sub", "xor",
  "pow", "copysign",
  "uaddo", "umulo", "usubo",
  "saddo", "smulo", "ssubo",
  "phi",
  "set",
  "syscall",
  "clone",
  "x86_xchg",
  "x86_cmpxchg",
  "x86_rdtsc",
  "x86_fnstcw",
  "x86_fnstsw",
  "x86_fnstenv",
  "x86_fldcw",
  "x86_fldenv",
  "x86_ldmxcsr",
  "x86_stmxcsr",
  "x86_fnclex",
};

// -----------------------------------------------------------------------------
void Printer::Print(const Inst &inst)
{
  os_ << "\t" << kNames[static_cast<uint8_t>(inst.GetKind())];
  // Print the data width the instruction is operating on.
  if (auto size = inst.GetSize()) {
    os_ << "." << *size;
  }
  // Print instruction-specific attributes.
  switch (inst.GetKind()) {
    case Inst::Kind::INVOKE:
    case Inst::Kind::CALL: {
      os_ << ".";
      auto &call = static_cast<const CallSite &>(inst);
      Print(call.GetCallingConv());
      break;
    }
    case Inst::Kind::TCALL: {
      os_ << ".";
      auto &call = static_cast<const CallSite &>(inst);
      Print(call.GetCallingConv());
      for (const Type type : call.types()) {
        os_ << "."; Print(type);
      }
      break;
    }
    case Inst::Kind::CMP: {
      os_ << ".";
      switch (static_cast<const CmpInst &>(inst).GetCC()) {
        case Cond::EQ:  os_ << "eq";  break;
        case Cond::OEQ: os_ << "oeq"; break;
        case Cond::UEQ: os_ << "ueq"; break;
        case Cond::NE:  os_ << "ne";  break;
        case Cond::ONE: os_ << "one"; break;
        case Cond::UNE: os_ << "une"; break;
        case Cond::LT:  os_ << "lt";  break;
        case Cond::OLT: os_ << "olt"; break;
        case Cond::ULT: os_ << "ult"; break;
        case Cond::GT:  os_ << "gt";  break;
        case Cond::OGT: os_ << "ogt"; break;
        case Cond::UGT: os_ << "ugt"; break;
        case Cond::LE:  os_ << "le";  break;
        case Cond::OLE: os_ << "ole"; break;
        case Cond::ULE: os_ << "ule"; break;
        case Cond::GE:  os_ << "ge";  break;
        case Cond::OGE: os_ << "oge"; break;
        case Cond::UGE: os_ << "uge"; break;
      }
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
          os_ << i;
          return;
        }
        case Constant::Kind::REG: {
          switch (static_cast<const ConstantReg &>(c).GetValue()) {
            case ConstantReg::Kind::SP:         os_ << "$sp";         return;
            case ConstantReg::Kind::FS:         os_ << "$fs";         return;
            case ConstantReg::Kind::RET_ADDR:   os_ << "$ret_addr";   return;
            case ConstantReg::Kind::FRAME_ADDR: os_ << "$frame_addr"; return;
            case ConstantReg::Kind::PC:         os_ << "$pc";         return;
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
void Printer::Print(CallingConv conv)
{
  switch (conv) {
    case CallingConv::C:          os_ << "c";          break;
    case CallingConv::CAML:       os_ << "caml";       break;
    case CallingConv::CAML_ALLOC: os_ << "caml_alloc"; break;
    case CallingConv::CAML_GC:    os_ << "caml_gc";    break;
    case CallingConv::SETJMP:     os_ << "setjmp";     break;
  }
}

// -----------------------------------------------------------------------------
void Printer::Print(Visibility visibility)
{
  switch (visibility) {
    case Visibility::LOCAL:           os_ << "local";           break;
    case Visibility::GLOBAL_DEFAULT:  os_ << "global_default";  break;
    case Visibility::GLOBAL_HIDDEN:   os_ << "global_hidden";   break;
    case Visibility::WEAK_DEFAULT:    os_ << "weak_default";    break;
    case Visibility::WEAK_HIDDEN:     os_ << "weak_hidden";     break;
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
