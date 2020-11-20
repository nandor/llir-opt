// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/BinaryFormat/ELF.h>
#include <llvm/CodeGen/MachineModuleInfo.h>
#include <llvm/IR/Mangler.h>
#include <llvm/MC/MCSectionELF.h>
#include <llvm/MC/MCInstBuilder.h>
#include <llvm/Target/RISCV/RISCVInstrInfo.h>
#include <llvm/Target/RISCV/MCTargetDesc/RISCVMCExpr.h>

#include "core/cast.h"
#include "core/data.h"
#include "core/prog.h"
#include "core/block.h"
#include "core/func.h"
#include "emitter/riscv/riscvruntime_printer.h"

using MCSymbol = llvm::MCSymbol;
using MCInst = llvm::MCInst;
using MCOperand = llvm::MCOperand;
using MCInstBuilder = llvm::MCInstBuilder;
using RISCVMCExpr = llvm::RISCVMCExpr;
namespace RISCV = llvm::RISCV;



// -----------------------------------------------------------------------------
char RISCVRuntimePrinter::ID;

// -----------------------------------------------------------------------------
RISCVRuntimePrinter::RISCVRuntimePrinter(
    const Prog &prog,
    llvm::MCContext *ctx,
    llvm::MCStreamer *os,
    const llvm::MCObjectFileInfo *objInfo,
    const llvm::DataLayout &layout,
    const llvm::RISCVSubtarget &sti,
    bool shared)
  : RuntimePrinter(ID, prog, ctx, os, objInfo, layout, shared)
  , sti_(sti)
{
}

// -----------------------------------------------------------------------------
llvm::StringRef RISCVRuntimePrinter::getPassName() const
{
  return "LLIR RISCV Data Section Printer";
}

// -----------------------------------------------------------------------------
static std::vector<llvm::Register> kXRegs =
{
  RISCV::X5,  RISCV::X6,  RISCV::X7,
  RISCV::X8,  RISCV::X9,  RISCV::X10,
  RISCV::X11, RISCV::X12, RISCV::X13,
  RISCV::X14, RISCV::X15, RISCV::X16,
  RISCV::X17, RISCV::X18, RISCV::X19,
  RISCV::X20, RISCV::X21, RISCV::X22,
  RISCV::X23, RISCV::X24, RISCV::X25,
  RISCV::X26, RISCV::X27, RISCV::X28,
  RISCV::X29, RISCV::X30, RISCV::X31,
};

// -----------------------------------------------------------------------------
static std::vector<llvm::Register> kDRegs =
{
  RISCV::F0_D,  RISCV::F1_D,  RISCV::F2_D,  RISCV::F3_D,
  RISCV::F4_D,  RISCV::F5_D,  RISCV::F6_D,  RISCV::F7_D,
  RISCV::F8_D,  RISCV::F9_D,  RISCV::F10_D, RISCV::F11_D,
  RISCV::F12_D, RISCV::F13_D, RISCV::F14_D, RISCV::F15_D,
  RISCV::F16_D, RISCV::F17_D, RISCV::F18_D, RISCV::F19_D,
  RISCV::F20_D, RISCV::F21_D, RISCV::F22_D, RISCV::F23_D,
  RISCV::F24_D, RISCV::F25_D, RISCV::F26_D, RISCV::F27_D,
  RISCV::F28_D, RISCV::F29_D, RISCV::F30_D, RISCV::F31_D,
};

// -----------------------------------------------------------------------------
void RISCVRuntimePrinter::EmitCamlCallGc()
{
  // caml_call_gc:
  auto *sym = LowerSymbol("caml_call_gc");
  os_->SwitchSection(objInfo_->getTextSection());
  os_->emitCodeAlignment(16);
  os_->emitLabel(sym);
  os_->emitSymbolAttribute(sym, llvm::MCSA_Global);

  StoreState(RISCV::X8, RISCV::X1, "last_return_address");
  StoreState(RISCV::X8, RISCV::X2, "bottom_of_stack");
  StoreState(RISCV::X8, RISCV::X9, "young_ptr");
  StoreState(RISCV::X8, RISCV::X26, "young_limit");

  // addi sp, sp, size
  os_->emitInstruction(MCInstBuilder(RISCV::ADDI)
      .addReg(RISCV::X2)
      .addReg(RISCV::X2)
      .addImm(- 8 * (kXRegs.size() + kDRegs.size())),
      sti_
  );

  // sd xi, (8 * i)(sp)
  for (unsigned i = 0, n = kXRegs.size(); i < n; ++i) {
    os_->emitInstruction(MCInstBuilder(RISCV::SD)
        .addReg(kXRegs[i])
        .addReg(RISCV::X2)
        .addImm(8 * i),
        sti_
    );
  }

  // fsd xi, (8 * i + 208)(sp)
  for (unsigned i = 0, n = kDRegs.size(); i < n; ++i) {
    os_->emitInstruction(MCInstBuilder(RISCV::FSD)
        .addReg(kDRegs[i])
        .addReg(RISCV::X2)
        .addImm(8 * (i + kXRegs.size())),
        sti_
    );
  }

  StoreState(RISCV::X8, RISCV::X2, "gc_regs");

  // jal ra, caml_garbage_collection
  os_->emitInstruction(
      MCInstBuilder(RISCV::PseudoCALL)
        .addExpr(RISCVMCExpr::create(
            llvm::MCSymbolRefExpr::create(
                LowerSymbol("caml_garbage_collection"),
                *ctx_
            ),
            RISCVMCExpr::VK_RISCV_CALL,
            *ctx_
        )),
        sti_
  );

  // fld xi, (8 * i + 208)(sp)
  for (unsigned i = 0, n = kDRegs.size(); i < n; ++i) {
    os_->emitInstruction(MCInstBuilder(RISCV::FLD)
        .addReg(kDRegs[i])
        .addReg(RISCV::X2)
        .addImm(8 * (i + kXRegs.size())),
        sti_
    );
  }

  // ld xi, (8 * i)(sp)
  for (unsigned i = 0, n = kXRegs.size(); i < n; ++i) {
    os_->emitInstruction(MCInstBuilder(RISCV::LD)
        .addReg(kXRegs[i])
        .addReg(RISCV::X2)
        .addImm(8 * i),
        sti_
    );
  }

  // addi sp, sp, size
  os_->emitInstruction(MCInstBuilder(RISCV::ADDI)
      .addReg(RISCV::X2)
      .addReg(RISCV::X2)
      .addImm(8 * (kXRegs.size() + kDRegs.size())),
      sti_
  );

  // la x8, Caml_state
  LoadCamlState(RISCV::X8);
  // ldr x9, [x8, #young_ptr]
  LoadState(RISCV::X8, RISCV::X9, "young_ptr");
  // ldr x26, [x8, #young_limit]
  LoadState(RISCV::X8, RISCV::X26, "young_limit");
  // ldr x27, [x8, #last_return_address]
  LoadState(RISCV::X8, RISCV::X1, "last_return_address");

  // ret
  os_->emitInstruction(MCInstBuilder(RISCV::JALR)
      .addReg(RISCV::X0)
      .addReg(RISCV::X1)
      .addImm(0),
      sti_
  );
}

// -----------------------------------------------------------------------------
static const std::unordered_map<std::string, unsigned> kOffsets =
{
  #define FIELD(name, index) { #name, index },
  #include "core/state.h"
  #undef FIELD
};

// -----------------------------------------------------------------------------
static unsigned GetOffset(const char *name)
{
  auto it = kOffsets.find(name);
  assert(it != kOffsets.end() && "missing offset");
  return it->second;
}

// -----------------------------------------------------------------------------
void RISCVRuntimePrinter::EmitCamlCCall()
{
  // caml_c_call:
  auto *sym = LowerSymbol("caml_c_call");
  os_->SwitchSection(objInfo_->getTextSection());
  os_->emitCodeAlignment(16);
  os_->emitLabel(sym);
  os_->emitSymbolAttribute(sym, llvm::MCSA_Global);

  LoadCamlState(RISCV::X8);

  // str x2, bottom_of_stack(x8)
  StoreState(RISCV::X8, RISCV::X2, "bottom_of_stack");
  // str x1, last_return_address(x8)
  StoreState(RISCV::X8, RISCV::X1, "last_return_address");

  os_->emitInstruction(
      MCInstBuilder(RISCV::JALR)
        .addReg(RISCV::X0)
        .addReg(RISCV::X7)
        .addImm(0),
      sti_
  );
}

// -----------------------------------------------------------------------------
MCSymbol *RISCVRuntimePrinter::LowerSymbol(const char *name)
{
  llvm::SmallString<128> sym;
  llvm::Mangler::getNameWithPrefix(sym, name, layout_);
  return ctx_->getOrCreateSymbol(sym);
}

// -----------------------------------------------------------------------------
void RISCVRuntimePrinter::LoadCamlState(llvm::Register state)
{
  // Caml_state reference.
  auto *sym = llvm::MCSymbolRefExpr::create(LowerSymbol("Caml_state"), *ctx_);
  auto *pc = ctx_->createTempSymbol();

  // lbl:
  os_->emitLabel(pc);

  // auipc x8, %pcrel_hi(Caml_state)(x8)
  os_->emitInstruction(MCInstBuilder(RISCV::AUIPC)
      .addReg(state)
      .addExpr(RISCVMCExpr::create(
          sym,
          RISCVMCExpr::VK_RISCV_PCREL_HI,
          *ctx_
      )),
      sti_
  );
  // add x8, x8, %pcrel_lo(Caml_state)
  os_->emitInstruction(MCInstBuilder(RISCV::ADDI)
      .addReg(state)
      .addReg(state)
      .addExpr(RISCVMCExpr::create(
          llvm::MCSymbolRefExpr::create(pc, *ctx_),
          RISCVMCExpr::VK_RISCV_PCREL_LO,
          *ctx_
      )),
      sti_
  );
  // ld x8, 0(x8)
  os_->emitInstruction(MCInstBuilder(RISCV::LD)
      .addReg(state)
      .addReg(state)
      .addImm(0),
      sti_
  );
}

// -----------------------------------------------------------------------------
void RISCVRuntimePrinter::StoreState(
    llvm::Register state,
    llvm::Register val,
    const char *name)
{
  os_->emitInstruction(
      MCInstBuilder(RISCV::SD)
        .addReg(val)
        .addReg(state)
        .addImm(GetOffset(name) * 8),
      sti_
  );
}

// -----------------------------------------------------------------------------
void RISCVRuntimePrinter::LoadState(
    llvm::Register state,
    llvm::Register val,
    const char *name)
{
  os_->emitInstruction(
      MCInstBuilder(RISCV::LD)
        .addReg(val)
        .addReg(state)
        .addImm(GetOffset(name) * 8),
      sti_
  );
}
