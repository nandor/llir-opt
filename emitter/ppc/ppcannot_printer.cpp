// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <sstream>

#include <llvm/Target/PowerPC/PPCInstrInfo.h>

#include "emitter/ppc/ppcannot_printer.h"

namespace PPC = llvm::PPC;
namespace TargetOpcode = llvm::TargetOpcode;
using TargetInstrInfo = llvm::TargetInstrInfo;
using StackVal = llvm::FixedStackPseudoSourceValue;



// -----------------------------------------------------------------------------
char PPCAnnotPrinter::ID;

// -----------------------------------------------------------------------------
static const char *kRegNames[] =
{
   "x3",  "x4",  "x5",  "x6",  "x7",
   "x8",  "x9", "x10", "x11", "x12",
  "x13", "x14", "x15", "x16", "x17",
  "x18", "x19", "x20", "x21", "x22",
  "x23", "x24", "x25", "x26", "x27"
};

// -----------------------------------------------------------------------------
PPCAnnotPrinter::PPCAnnotPrinter(
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
PPCAnnotPrinter::~PPCAnnotPrinter()
{
}

// -----------------------------------------------------------------------------
std::optional<unsigned> PPCAnnotPrinter::GetRegisterIndex(llvm::Register reg)
{
  switch (reg) {
    case PPC::X3:  return 0;
    case PPC::X4:  return 1;
    case PPC::X5:  return 2;
    case PPC::X6:  return 3;
    case PPC::X7:  return 4;
    case PPC::X8:  return 5;
    case PPC::X9:  return 6;
    case PPC::X10: return 7;
    case PPC::X11: return 8;
    case PPC::X12: return 9;
    case PPC::X13: return 10;
    case PPC::X14: return 11;
    case PPC::X15: return 12;
    case PPC::X16: return 13;
    case PPC::X17: return 14;
    case PPC::X18: return 15;
    case PPC::X19: return 16;
    case PPC::X20: return 17;
    case PPC::X21: return 18;
    case PPC::X22: return 19;
    case PPC::X23: return 20;
    case PPC::X24: return 21;
    case PPC::X25: return 22;
    case PPC::X26: return 23;
    case PPC::X27: return 24;
    default: return std::nullopt;
  }
}

// -----------------------------------------------------------------------------
llvm::StringRef PPCAnnotPrinter::GetRegisterName(unsigned reg)
{
  assert((reg < (sizeof(kRegNames) / sizeof(kRegNames[0]))) && "invalid reg");
  return kRegNames[reg];
}

// -----------------------------------------------------------------------------
llvm::Register PPCAnnotPrinter::GetStackPointer()
{
  return PPC::X1;
}

// -----------------------------------------------------------------------------
int64_t PPCAnnotPrinter::GetFrameOffset(const llvm::MachineInstr &MI) const
{
  auto prev = std::prev(MI.getIterator());
  switch (prev->getOpcode()) {
    case PPC::BCTRL8_LDinto_toc:
    case PPC::BL8_NOP: {
      return -4;
    }
    case PPC::BL8: {
      return 0;
    }
    default: {
      llvm_unreachable("invalid instruction");
    }
  }
}

// -----------------------------------------------------------------------------
llvm::StringRef PPCAnnotPrinter::getPassName() const
{
  return "LLIR PPC Annotation Inserter";
}
