// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/context.h"
#include "core/block.h"
#include "core/func.h"
#include "core/printer.h"
#include "core/prog.h"



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
  "call", "tcall", "jt", "jf", "ji", "jmp", "ret", "switch",
  "ld", "st", "push", "pop",
  "xchg",
  "set",
  "imm", "addr", "arg",
  "select",
  "abs", "mov", "neg", "sext", "zext", "trunc",
  "add", "and", "asr", "cmp", "div", "lsl", "lsr", "mod", "mul",
  "mulh", "or", "rotl", "shl", "sra", "rem", "srl", "sub", "xor",
  "phi",
};

// -----------------------------------------------------------------------------
void Printer::Print(const Inst *inst)
{
  os_ << "\t" << kNames[static_cast<uint8_t>(inst->GetKind())] << "\t";
  if (auto numRet = inst->GetNumRets()) {
    os_ << "$" << insts_[inst];
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
    case Operand::Kind::INST: {
      auto inst = op.GetInst();
      assert(!(reinterpret_cast<uintptr_t>(inst) & 1) && "invalid inst");
      os_ << "$" << insts_[inst];
      break;
    }
    case Operand::Kind::SYM: {
      os_ << op.GetSym()->GetName();
      break;
    }
    case Operand::Kind::EXPR: {
      Print(op.GetExpr());
      break;
    }
    case Operand::Kind::BLOCK: {
      os_ << op.GetBlock()->GetName();
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
