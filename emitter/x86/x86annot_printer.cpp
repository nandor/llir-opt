// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <sstream>

#include <llvm/Target/X86/X86InstrInfo.h>

#include "emitter/x86/x86annot_printer.h"

namespace X86 = llvm::X86;
namespace TargetOpcode = llvm::TargetOpcode;
using TargetInstrInfo = llvm::TargetInstrInfo;
using StackVal = llvm::FixedStackPseudoSourceValue;



// -----------------------------------------------------------------------------
char X86AnnotPrinter::ID;


// -----------------------------------------------------------------------------
static const char *kRegNames[] =
{
  "rax", "rbx", "rdi", "rsi", "rdx", "rcx", "r8",
  "r9", "r12", "r13", "r10", "r11", "rbp", "r14", "r15"
};

// -----------------------------------------------------------------------------
X86AnnotPrinter::X86AnnotPrinter(
    llvm::MCContext *ctx,
    llvm::MCStreamer *os,
    const llvm::MCObjectFileInfo *objInfo,
    const llvm::DataLayout &layout,
    const ISelMapping &mapping,
    bool shared)
  : AnnotPrinter(ID, ctx, os, objInfo, layout, mapping, shared)
{
}

// -----------------------------------------------------------------------------
X86AnnotPrinter::~X86AnnotPrinter()
{
}

// -----------------------------------------------------------------------------
std::optional<unsigned> X86AnnotPrinter::GetRegisterIndex(llvm::Register reg)
{
  switch (reg) {
    case X86::RAX: return 0;
    case X86::RBX: return 1;
    case X86::RDI: return 2;
    case X86::RSI: return 3;
    case X86::RDX: return 4;
    case X86::RCX: return 5;
    case X86::R8:  return 6;
    case X86::R9:  return 7;
    case X86::R12: return 8;
    case X86::R13: return 9;
    case X86::R10: return 10;
    case X86::R11: return 11;
    case X86::RBP: return 12;
    case X86::R14: return 13;
    case X86::R15: return 14;
    default: return std::nullopt;
  }
}

// -----------------------------------------------------------------------------
llvm::StringRef X86AnnotPrinter::GetRegisterName(unsigned reg)
{
  assert((reg < (sizeof(kRegNames) / sizeof(kRegNames[0]))) && "invalid reg");
  return kRegNames[reg];
}

// -----------------------------------------------------------------------------
llvm::Register X86AnnotPrinter::GetStackPointer()
{
  return X86::RSP;
}
