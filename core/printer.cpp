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
#include "core/insts_binary.h"
#include "core/insts_call.h"
#include "core/prog.h"
#include "core/extern.h"



// -----------------------------------------------------------------------------
void Printer::Print(const Prog &prog)
{
  // Print the text segment.
  os_ << "\t.code\n";
  for (const Func &f : prog) {
    Print(f);
  }
  os_ << "\n";

  // Print all data segments.
  for (const Data &data : prog.data()) {
    os_ << "\t.data\t" << data.getName() << "\n";
    Print(data);
    os_ << "\n";
  }
}

// -----------------------------------------------------------------------------
void Printer::Print(const Data &data)
{
  for (auto &atom : data) {
    Print(atom);
  }
}

// -----------------------------------------------------------------------------
void Printer::Print(const Atom &atom)
{
  os_ << "\t.align\t" << atom.GetAlignment() << "\n";
  os_ << atom.getName() << ":\n";
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
        os_ << "\t.ascii\t\"";
        for (const uint8_t c : item.GetString()) {
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
        break;
      }
      case Item::Kind::END: {
        os_ << "\t.end";
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
  os_ << "\t.visibility\t"; Print(func.GetVisibility()); os_ << "\n";
  os_ << "\t.call\t"; Print(func.GetCallingConv()); os_ << "\n";

  for (auto &o : func.objects()) {
    os_ << "\t.stack_object\t";
    os_ << o.Index << ", " << o.Size << ", " << o.Alignment;
    os_ << "\n";
  }

  os_ << "\t.args\t" << func.IsVarArg();
  for (const auto type : func.params()) {
    os_ << ", "; Print(type);
  }
  os_ << "\n";
  {
    llvm::ReversePostOrderTraversal<const Func *> rpot(&func);
    for (const Block *b : rpot) {
      for (const Inst &i : *b) {
        insts_.emplace(&i, insts_.size());
      }
    }
    for (const Block *b : rpot) {
      Print(*b);
    }
  }
  insts_.clear();
  os_ << "\n";
}

// -----------------------------------------------------------------------------
void Printer::Print(const Block &block)
{
  os_ << block.getName() << ":\n";
  for (const Inst &i : block) {
    Print(i);
  }
}

// -----------------------------------------------------------------------------
static const char *kNames[] =
{
  "call", "tcall", "invoke", "tinvoke", "ret",
  "jcc", "ji", "jmp", "switch", "trap",
  "ld", "st",
  "xchg",
  "set",
  "vastart",
  "alloca",
  "arg", "frame", "undef",
  "rdtsc",
  "select",
  "abs", "neg",  "sqrt", "sin", "cos",
  "sext", "zext", "fext",
  "mov", "trunc",
  "exp", "exp2", "log", "log2", "log10",
  "fceil", "ffloor",
  "popcnt",
  "clz",
  "add", "and", "cmp", "div", "rem", "mul", "or",
  "rotl", "rotr",
  "sll", "sra", "srl", "sub", "xor",
  "pow", "copysign",
  "uaddo", "umulo", "usubo",
  "saddo", "smulo", "ssubo",
  "phi",
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
    case Inst::Kind::TINVOKE:
    case Inst::Kind::TCALL: {
      os_ << ".";
      auto &term = static_cast<const CallSite<TerminatorInst> &>(inst);
      Print(term.GetCallingConv());
      if (auto type = term.GetType(); type && term.GetNumRets() == 0) {
        os_ << "."; Print(*type);
      }
      break;
    }
    case Inst::Kind::CALL: {
      os_ << ".";
      Print(static_cast<const CallInst &>(inst).GetCallingConv());
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
    auto it = insts_.find(&inst);
    if (it == insts_.end()) {
      os_ << "$<" << &inst << ">";
    } else {
      os_ << "$" << it->second;
    }
  } else {
    os_ << "\t";
  }
  // Print the arguments.
  for (auto it = inst.value_op_begin(); it != inst.value_op_end(); ++it) {
    if (inst.GetNumRets() || it != inst.value_op_begin()) {
      os_ << ", ";
    }
    Print(**it);
  }
  // Print any annotations.
  for (const auto &annot : inst.annots()) {
    os_ << " ";
    switch (annot) {
      case CAML_FRAME:   os_ << "@caml_frame";   break;
      case CAML_ROOT:    os_ << "@caml_root";    break;
      case CAML_VALUE:   os_ << "@caml_value";   break;
      case CAML_ADDR:    os_ << "@caml_addr";    break;
    }
  }
  os_ << "\n";
}

// -----------------------------------------------------------------------------
void Printer::Print(const Value &val)
{
  if (reinterpret_cast<uintptr_t>(&val) & 1) {
    os_ << "<" << (reinterpret_cast<uintptr_t>(&val) >> 1) << ">";
    return;
  }

  switch (val.GetKind()) {
    case Value::Kind::INST: {
      auto it = insts_.find(static_cast<const Inst *>(&val));
      if (it == insts_.end()) {
        os_ << "$<" << &val << ">";
      } else {
        os_ << "$" << it->second;
      }
      break;
    }
    case Value::Kind::GLOBAL: {
      os_ << static_cast<const Global &>(val).getName();
      break;
    }
    case Value::Kind::EXPR: {
      Print(static_cast<const Expr &>(val));
      break;
    }
    case Value::Kind::CONST: {
      switch (static_cast<const Constant &>(val).GetKind()) {
        case Constant::Kind::INT: {
          os_ << static_cast<const ConstantInt &>(val).GetValue();
          break;
        }
        case Constant::Kind::FLOAT: {
          union { double d; int64_t i; };
          d = static_cast<const ConstantFloat &>(val).GetDouble();
          os_ << i;
          break;
        }
        case Constant::Kind::REG: {
          switch (static_cast<const ConstantReg &>(val).GetValue()) {
            case ConstantReg::Kind::RAX:        os_ << "$rax";        break;
            case ConstantReg::Kind::RBX:        os_ << "$rbx";        break;
            case ConstantReg::Kind::RCX:        os_ << "$rcx";        break;
            case ConstantReg::Kind::RDX:        os_ << "$rdx";        break;
            case ConstantReg::Kind::RSI:        os_ << "$rsi";        break;
            case ConstantReg::Kind::RDI:        os_ << "$rdi";        break;
            case ConstantReg::Kind::RSP:        os_ << "$rsp";        break;
            case ConstantReg::Kind::RBP:        os_ << "$rbp";        break;
            case ConstantReg::Kind::R8:         os_ << "$r8";         break;
            case ConstantReg::Kind::R9:         os_ << "$r9";         break;
            case ConstantReg::Kind::R10:        os_ << "$r10";        break;
            case ConstantReg::Kind::R11:        os_ << "$r11";        break;
            case ConstantReg::Kind::R12:        os_ << "$r12";        break;
            case ConstantReg::Kind::R13:        os_ << "$r13";        break;
            case ConstantReg::Kind::R14:        os_ << "$r14";        break;
            case ConstantReg::Kind::R15:        os_ << "$r15";        break;
            case ConstantReg::Kind::RET_ADDR:   os_ << "$ret_addr";   break;
            case ConstantReg::Kind::FRAME_ADDR: os_ << "$frame_addr"; break;
            case ConstantReg::Kind::PC:         os_ << "$pc";         break;
          }
          break;
        }
      }
      break;
    }
  }
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
    case CallingConv::FAST:       os_ << "fast";       break;
    case CallingConv::CAML:       os_ << "caml";       break;
    case CallingConv::CAML_ALLOC: os_ << "caml_alloc"; break;
    case CallingConv::CAML_GC:    os_ << "caml_gc";    break;
    case CallingConv::CAML_RAISE: os_ << "caml_raise"; break;
  }
}

// -----------------------------------------------------------------------------
void Printer::Print(Visibility visibility)
{
  switch (visibility) {
    case Visibility::EXTERN: os_ << "extern"; break;
    case Visibility::HIDDEN: os_ << "hidden"; break;
  }
}
