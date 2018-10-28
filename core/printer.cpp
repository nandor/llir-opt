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
  os_ << "\t" << kNames[static_cast<uint8_t>(inst->GetKind())] << std::endl;
}
