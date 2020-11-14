// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <sstream>

#include <llvm/Target/RISCV/RISCVInstrInfo.h>
#include <llvm/Target/RISCV/MCTargetDesc/RISCVMCTargetDesc.h>

#include "emitter/riscv/riscvannot_printer.h"

namespace RISCV = llvm::RISCV;
namespace TargetOpcode = llvm::TargetOpcode;
using TargetInstrInfo = llvm::TargetInstrInfo;
using StackVal = llvm::FixedStackPseudoSourceValue;



// -----------------------------------------------------------------------------
char RISCVAnnotPrinter::ID;

// -----------------------------------------------------------------------------
static const char *kRegNames[] =
{
   "x5",  "x6",  "x7",  "x8",  "x9", "x10", "x11", "x12", "x13",
  "x14", "x15", "x16", "x17", "x18", "x19", "x20", "x21", "x22",
  "x23", "x24", "x25", "x26", "x27", "x28", "x29", "x30", "x31",
};

// -----------------------------------------------------------------------------
RISCVAnnotPrinter::RISCVAnnotPrinter(
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
RISCVAnnotPrinter::~RISCVAnnotPrinter()
{
}

// -----------------------------------------------------------------------------
std::optional<unsigned> RISCVAnnotPrinter::GetRegisterIndex(llvm::Register reg)
{
  switch (reg) {
    case RISCV::X5:  return 0;
    case RISCV::X6:  return 1;
    case RISCV::X7:  return 2;
    case RISCV::X8:  return 3;
    case RISCV::X9:  return 4;
    case RISCV::X10: return 5;
    case RISCV::X11: return 6;
    case RISCV::X12: return 7;
    case RISCV::X13: return 8;
    case RISCV::X14: return 9;
    case RISCV::X15: return 10;
    case RISCV::X16: return 11;
    case RISCV::X17: return 12;
    case RISCV::X18: return 13;
    case RISCV::X19: return 14;
    case RISCV::X20: return 15;
    case RISCV::X21: return 16;
    case RISCV::X22: return 17;
    case RISCV::X23: return 18;
    case RISCV::X24: return 19;
    case RISCV::X25: return 20;
    case RISCV::X26: return 21;
    case RISCV::X27: return 22;
    case RISCV::X28: return 23;
    case RISCV::X29: return 24;
    case RISCV::X30: return 25;
    case RISCV::X31: return 26;
    default: return std::nullopt;
  }
}

// -----------------------------------------------------------------------------
llvm::StringRef RISCVAnnotPrinter::GetRegisterName(unsigned reg)
{
  assert((reg < (sizeof(kRegNames) / sizeof(kRegNames[0]))) && "invalid reg");
  return kRegNames[reg];
}

// -----------------------------------------------------------------------------
llvm::Register RISCVAnnotPrinter::GetStackPointer()
{
  return RISCV::X2;
}

// -----------------------------------------------------------------------------
llvm::StringRef RISCVAnnotPrinter::getPassName() const
{
  return "LLIR RISCV Annotation Inserter";
}
