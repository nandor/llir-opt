// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <sstream>

#include <llvm/Target/AArch64/AArch64InstrInfo.h>

#include "emitter/aarch64/aarch64annot_printer.h"

namespace AArch64 = llvm::AArch64;
namespace TargetOpcode = llvm::TargetOpcode;
using TargetInstrInfo = llvm::TargetInstrInfo;
using StackVal = llvm::FixedStackPseudoSourceValue;



// -----------------------------------------------------------------------------
char AArch64AnnotPrinter::ID;

// -----------------------------------------------------------------------------
static const char *kRegNames[] =
{
  "x0",  "x1",  "x2",  "x3",  "x4",  "x5",  "x6",  "x7",
  "x8",  "x9",  "x10", "x11", "x12", "x13", "x14", "x15",
  "x19", "x20", "x21", "x22", "x23", "x24",
  "x25", "x26", "x27", "x28", "x16", "x17"
};

// -----------------------------------------------------------------------------
AArch64AnnotPrinter::AArch64AnnotPrinter(
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
AArch64AnnotPrinter::~AArch64AnnotPrinter()
{
}

// -----------------------------------------------------------------------------
std::optional<unsigned> AArch64AnnotPrinter::GetRegisterIndex(llvm::Register reg)
{
  switch (reg) {

    case AArch64::X0: return 0;
    case AArch64::X1: return 1;
    case AArch64::X2: return 2;
    case AArch64::X3: return 3;
    case AArch64::X4: return 4;
    case AArch64::X5: return 5;
    case AArch64::X6: return 6;
    case AArch64::X7: return 7;
    case AArch64::X8: return 8;
    case AArch64::X9: return 9;
    case AArch64::X10: return 10;
    case AArch64::X11: return 11;
    case AArch64::X12: return 12;
    case AArch64::X13: return 13;
    case AArch64::X14: return 14;
    case AArch64::X15: return 15;
    case AArch64::X19: return 16;
    case AArch64::X20: return 17;
    case AArch64::X21: return 18;
    case AArch64::X22: return 19;
    case AArch64::X23: return 20;
    case AArch64::X24: return 21;
    case AArch64::X25: return 22;
    case AArch64::X26: return 23;
    case AArch64::X27: return 24;
    case AArch64::X28: return 25;
    case AArch64::X16: return 26;
    case AArch64::X17: return 27;
    default: return std::nullopt;
  }
}

// -----------------------------------------------------------------------------
llvm::StringRef AArch64AnnotPrinter::GetRegisterName(unsigned reg)
{
  assert((reg < (sizeof(kRegNames) / sizeof(kRegNames[0]))) && "invalid reg");
  return kRegNames[reg];
}

// -----------------------------------------------------------------------------
llvm::Register AArch64AnnotPrinter::GetStackPointer()
{
  return AArch64::SP;
}
