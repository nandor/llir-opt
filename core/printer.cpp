// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/context.h"
#include "core/block.h"
#include "core/func.h"
#include "core/printer.h"
#include "core/prog.h"
#include "core/symbol.h"



// -----------------------------------------------------------------------------
void Printer::Print(const Prog *prog)
{
  for (const Func &f : *prog) {
    Print(&f);
  }
}

// -----------------------------------------------------------------------------
void Printer::Print(const Func *func)
{
  os_ << func->GetName() << ":" << std::endl;
  if (auto stackSize = func->GetStackSize()) {
    os_ << "\t.stack\t" << stackSize << std::endl;
  }
  for (const Block &b : *func) {
    Print(&b);
  }
  insts_.clear();
}


// -----------------------------------------------------------------------------
void Printer::Print(const Block *block)
{
  os_ << block->GetName() << ":" << std::endl;
  for (const Inst &i : *block) {
    insts_.emplace(&i, insts_.size());
  }
  for (const Inst &i : *block) {
    Print(&i);
  }
}

// -----------------------------------------------------------------------------
const char *kNames[] =
{
  "call", "tcall", "invoke", "ret",
  "jcc", "ji", "jmp", "switch", "trap",
  "ld", "st", "push", "pop",
  "xchg",
  "set",
  "imm", "addr", "arg",
  "select",
  "abs", "mov", "neg", "sext", "zext", "trunc",
  "add", "and", "cmp", "div", "rem", "mul", "or",
  "rotl", "sll", "sra", "srl", "sub", "xor",
  "phi",
};

// -----------------------------------------------------------------------------
void Printer::Print(const Inst *inst)
{
  os_ << "\t" << kNames[static_cast<uint8_t>(inst->GetKind())];
  if (auto size = inst->GetSize()) {
    os_ << "." << *size;
  }
  if (auto numRet = inst->GetNumRets()) {
    for (unsigned i = 0; i < numRet; ++i) {
      os_ << ".";
      Print(inst->GetType(i));
    }
    os_ << "\t";
    os_ << "$" << insts_[inst];
  } else {
    os_ << "\t";
  }
  for (unsigned i = 0, numOps = inst->GetNumOps(); i < numOps; ++i) {
    if ((i == 0 && inst->GetNumRets()) || i > 0) {
      os_ << ", ";
    }
    Print(inst->GetOp(i));
  }
  os_ << std::endl;
}

// -----------------------------------------------------------------------------
void Printer::Print(const Operand &op)
{
  switch (op.GetKind()) {
    case Operand::Kind::INT: {
      os_ << op.GetInt();
      break;
    }
    case Operand::Kind::FLOAT: {
      os_ << op.GetFloat();
      break;
    }
    case Operand::Kind::REG: {
      switch (op.GetReg()) {
        case Reg::SP: os_ << "$sp"; break;
        case Reg::FP: os_ << "$fp"; break;
      }
      break;
    }
    case Operand::Kind::VALUE: {
      Print(static_cast<Value *>(op.GetValue()));
      break;
    }
    case Operand::Kind::UNDEF: {
      os_ << "$undef";
      break;
    }
  }
}

// -----------------------------------------------------------------------------
void Printer::Print(const Value *val)
{
  switch (val->GetKind()) {
    case Value::Kind::INST: {
      auto inst = static_cast<const Inst *>(val);
      if (reinterpret_cast<uintptr_t>(inst) & 1) {
        os_ << "WTF";
      } else {
        assert(!(reinterpret_cast<uintptr_t>(inst) & 1) && "invalid inst");
        os_ << "$" << insts_[inst];
      }
      break;
    }
    case Value::Kind::SYMBOL: {
      os_ << static_cast<const Symbol *>(val)->GetName();
      break;
    }
    case Value::Kind::EXPR: {
      Print(static_cast<const Expr *>(val));
      break;
    }
    case Value::Kind::BLOCK: {
      os_ << static_cast<const Block *>(val)->GetName();
      break;
    }
    case Value::Kind::FUNC: {
      os_ << static_cast<const Func *>(val)->GetName();
      break;
    }
  }
}

// -----------------------------------------------------------------------------
void Printer::Print(const Expr *expr)
{
  switch (expr->GetKind()) {
    case Expr::Kind::SYMBOL_OFFSET: {
      auto *symExpr = static_cast<const SymbolOffsetExpr *>(expr);
      os_ << symExpr->GetSymbol()->GetName();
      int64_t off = symExpr->GetOffset();
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
  switch (type) {
    case Type::I8:  os_ << "i8";  break;
    case Type::I16: os_ << "i16"; break;
    case Type::I32: os_ << "i32"; break;
    case Type::I64: os_ << "i64"; break;
    case Type::U8:  os_ << "u8";  break;
    case Type::U16: os_ << "u16"; break;
    case Type::U32: os_ << "u32"; break;
    case Type::U64: os_ << "u64"; break;
    case Type::F32: os_ << "f32"; break;
    case Type::F64: os_ << "f64"; break;
  }
}
