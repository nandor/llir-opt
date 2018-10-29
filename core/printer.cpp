// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

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
  for (const Block &b : *func) {
    Print(&b);
  }
}


// -----------------------------------------------------------------------------
void Printer::Print(const Block *block)
{
  os_ << block->GetName() << ":" << std::endl;
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
    os_ << "RET";
  }

  // TODO: print destination
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
      os_ << "FLOAT";
      break;
    }
    case Operand::Kind::REG: {
      os_ << "REG";
      break;
    }
    case Operand::Kind::INST: {
      os_ << "INST";
      break;
    }
    case Operand::Kind::SYM: {
      os_ << "SYM";
      break;
    }
    case Operand::Kind::EXPR: {
      os_ << "EXPR";
      break;
    }
    case Operand::Kind::BLOCK: {
      os_ << op.GetBlock()->GetName();
      break;
    }
  }
}
