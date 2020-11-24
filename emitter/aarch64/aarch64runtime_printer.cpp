// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/BinaryFormat/ELF.h>
#include <llvm/CodeGen/MachineModuleInfo.h>
#include <llvm/IR/Mangler.h>
#include <llvm/MC/MCSectionELF.h>
#include <llvm/MC/MCInstBuilder.h>
#include <llvm/Target/AArch64/AArch64InstrInfo.h>
#include <llvm/Target/AArch64/MCTargetDesc/AArch64MCExpr.h>
#include <llvm/Target/AArch64/MCTargetDesc/AArch64AddressingModes.h>

#include "core/cast.h"
#include "core/data.h"
#include "core/prog.h"
#include "core/block.h"
#include "core/func.h"
#include "emitter/aarch64/aarch64runtime_printer.h"

using MCSymbol = llvm::MCSymbol;
using MCInst = llvm::MCInst;
using MCOperand = llvm::MCOperand;
using MCInstBuilder = llvm::MCInstBuilder;
using AArch64MCExpr = llvm::AArch64MCExpr;
namespace AArch64_AM = llvm::AArch64_AM;
namespace AArch64 = llvm::AArch64;



// -----------------------------------------------------------------------------
char AArch64RuntimePrinter::ID;

// -----------------------------------------------------------------------------
AArch64RuntimePrinter::AArch64RuntimePrinter(
    const Prog &prog,
    llvm::MCContext *ctx,
    llvm::MCStreamer *os,
    const llvm::MCObjectFileInfo *objInfo,
    const llvm::DataLayout &layout,
    const llvm::AArch64Subtarget &sti,
    bool shared)
  : RuntimePrinter(ID, prog, ctx, os, objInfo, layout, shared)
  , sti_(sti)
{
}

// -----------------------------------------------------------------------------
llvm::StringRef AArch64RuntimePrinter::getPassName() const
{
  return "LLIR AArch64 Data Section Printer";
}

// -----------------------------------------------------------------------------
static std::vector<std::pair<llvm::Register, llvm::Register>> kXRegs =
{
  { AArch64::X0, AArch64::X1 },
  { AArch64::X2, AArch64::X3 },
  { AArch64::X4, AArch64::X5 },
  { AArch64::X6, AArch64::X7 },
  { AArch64::X8, AArch64::X9 },
  { AArch64::X10, AArch64::X11 },
  { AArch64::X12, AArch64::X13 },
  { AArch64::X14, AArch64::X15 },
  { AArch64::X16, AArch64::X17 },
  { AArch64::X18, AArch64::X19 },
  { AArch64::X20, AArch64::X21 },
  { AArch64::X22, AArch64::X23 },
  { AArch64::X24, AArch64::FP },
};

// -----------------------------------------------------------------------------
static std::vector<std::pair<llvm::Register, llvm::Register>> kDRegs =
{
  { AArch64::D0, AArch64::D1 },
  { AArch64::D2, AArch64::D3 },
  { AArch64::D4, AArch64::D5 },
  { AArch64::D6, AArch64::D7 },
  { AArch64::D16, AArch64::D17 },
  { AArch64::D18, AArch64::D19 },
  { AArch64::D20, AArch64::D21 },
  { AArch64::D22, AArch64::D23 },
  { AArch64::D24, AArch64::D25 },
  { AArch64::D26, AArch64::D27 },
  { AArch64::D28, AArch64::D29 },
  { AArch64::D30, AArch64::D31 },
};

// -----------------------------------------------------------------------------
void AArch64RuntimePrinter::EmitCamlCallGc()
{
  // caml_call_gc:
  auto *sym = LowerSymbol("caml_call_gc");
  os_->SwitchSection(objInfo_->getTextSection());
  os_->emitCodeAlignment(16);
  os_->emitLabel(sym);
  os_->emitSymbolAttribute(sym, llvm::MCSA_Global);

  StoreState(AArch64::X25, AArch64::LR, "last_return_address");
  StoreState(AArch64::X25, AArch64::X26, "young_ptr");
  StoreState(AArch64::X25, AArch64::X27, "young_limit");
  StoreState(AArch64::X25, AArch64::X28, "exception_pointer");

  // add     x30, sp, 0
  os_->emitInstruction(MCInstBuilder(AArch64::ADDXri)
      .addReg(AArch64::LR)
      .addReg(AArch64::SP)
      .addImm(0)
      .addImm(AArch64_AM::getShiftValue(0)),
      sti_
  );

  // str     x30, [x25, bottom_of_stack]
  StoreState(AArch64::X25, AArch64::LR, "bottom_of_stack");


  // stp x0, x1, [sp, -#size]!
  os_->emitInstruction(MCInstBuilder(AArch64::STPXpre)
      .addReg(AArch64::SP)
      .addReg(kXRegs[0].first)
      .addReg(kXRegs[0].second)
      .addReg(AArch64::SP)
      .addImm(-2 * (kXRegs.size() + kDRegs.size())),
      sti_
  );

  // stp xn, xm, [sp, #off]
  for (int i = 1, n = kXRegs.size(); i < n; ++i) {
    auto [fst, snd] = kXRegs[i];
    os_->emitInstruction(MCInstBuilder(AArch64::STPXi)
        .addReg(fst)
        .addReg(snd)
        .addReg(AArch64::SP)
        .addImm(2 * i),
        sti_
    );
  }

  // stp dn, dm, [sp, #off]
  for (unsigned i = 0, n = kDRegs.size(); i < n; ++i) {
    auto [fst, snd] = kDRegs[i];
    os_->emitInstruction(MCInstBuilder(AArch64::STPDi)
        .addReg(fst)
        .addReg(snd)
        .addReg(AArch64::SP)
        .addImm(2 * (i + kXRegs.size())),
        sti_
    );
  }

  // add X30, sp, 0
  // str x30, [x16, #bottom_of_stack]
  os_->emitInstruction(MCInstBuilder(AArch64::ADDXri)
      .addReg(AArch64::LR)
      .addReg(AArch64::SP)
      .addImm(0)
      .addImm(AArch64_AM::getShiftValue(0)),
      sti_
  );
  StoreState(AArch64::X25, AArch64::LR, "gc_regs");

  // bl caml_garbage_collection
  os_->emitInstruction(MCInstBuilder(AArch64::BL)
      .addExpr(AArch64MCExpr::create(
          llvm::MCSymbolRefExpr::create(
              LowerSymbol("caml_garbage_collection"),
              *ctx_
          ),
          AArch64MCExpr::VK_ABS,
          *ctx_
      )),
      sti_
  );

  // ldp dn, dm, [sp, #off]
  for (int n = kDRegs.size(), i = n - 1; i >= 0; --i) {
    auto [fst, snd] = kDRegs[i];
    os_->emitInstruction(MCInstBuilder(AArch64::LDPDi)
        .addReg(fst)
        .addReg(snd)
        .addReg(AArch64::SP)
        .addImm(2 * (i + kXRegs.size())),
        sti_
    );
  }

  // ldp xn, xm, [sp, #off]
  for (int n = kXRegs.size(), i = n - 1; i >= 1; --i) {
    auto [fst, snd] = kXRegs[i];
    os_->emitInstruction(MCInstBuilder(AArch64::LDPXi)
        .addReg(fst)
        .addReg(snd)
        .addReg(AArch64::SP)
        .addImm(2 * i),
        sti_
    );
  }

  // ldp x0, x1, [sp], #off
  os_->emitInstruction(MCInstBuilder(AArch64::LDPXpost)
      .addReg(AArch64::SP)
      .addReg(kXRegs[0].first)
      .addReg(kXRegs[0].second)
      .addReg(AArch64::SP)
      .addImm(2 * (kXRegs.size() + kDRegs.size())),
      sti_
  );

  LoadCamlState(AArch64::X25);
  LoadState(AArch64::X25, AArch64::X26, "young_ptr");
  LoadState(AArch64::X25, AArch64::X27, "young_limit");
  LoadState(AArch64::X25, AArch64::X28, "exception_pointer");
  LoadState(AArch64::X25, AArch64::LR, "last_return_address");

  // ret
  os_->emitInstruction(MCInstBuilder(AArch64::RET).addReg(AArch64::LR), sti_);
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
void AArch64RuntimePrinter::EmitCamlCCall()
{
  // caml_c_call:
  auto *sym = LowerSymbol("caml_c_call");
  os_->SwitchSection(objInfo_->getTextSection());
  os_->emitCodeAlignment(16);
  os_->emitLabel(sym);
  os_->emitSymbolAttribute(sym, llvm::MCSA_Global);

  LoadCamlState(AArch64::X25);

  // add x26, sp, 0
  os_->emitInstruction(MCInstBuilder(AArch64::ADDXri)
      .addReg(AArch64::X26)
      .addReg(AArch64::SP)
      .addImm(0)
      .addImm(AArch64_AM::getShiftValue(0)),
      sti_
  );

  // str x26, [x25, #bottom_of_stack]
  StoreState(AArch64::X25, AArch64::X26, "bottom_of_stack");
  // str x30, [x25, #last_return_address]
  StoreState(AArch64::X25, AArch64::LR, "last_return_address");

  os_->emitInstruction(MCInstBuilder(AArch64::BR).addReg(AArch64::X15), sti_);
}

// -----------------------------------------------------------------------------
MCSymbol *AArch64RuntimePrinter::LowerSymbol(const char *name)
{
  llvm::SmallString<128> sym;
  llvm::Mangler::getNameWithPrefix(sym, name, layout_);
  return ctx_->getOrCreateSymbol(sym);
}

// -----------------------------------------------------------------------------
void AArch64RuntimePrinter::LoadCamlState(llvm::Register state)
{
  // Caml_state reference.
  auto *sym = llvm::MCSymbolRefExpr::create(LowerSymbol("Caml_state"), *ctx_);

  // adrp x25, Caml_state
  os_->emitInstruction(MCInstBuilder(AArch64::ADRP)
      .addReg(state)
      .addExpr(AArch64MCExpr::create(
          sym,
          AArch64MCExpr::VK_ABS,
          *ctx_
      )),
      sti_
  );
  // ldr x25, [x25, :lo12:Caml_state]
  os_->emitInstruction(MCInstBuilder(AArch64::LDRXui)
      .addReg(state)
      .addReg(state)
      .addExpr(AArch64MCExpr::create(
          sym,
          AArch64MCExpr::VK_LO12,
          *ctx_
      ))
      .addImm(AArch64_AM::getShiftValue(0)),
      sti_
  );
}

// -----------------------------------------------------------------------------
void AArch64RuntimePrinter::StoreState(
    llvm::Register state,
    llvm::Register val,
    const char *name)
{
  os_->emitInstruction(MCInstBuilder(AArch64::STRXui)
      .addReg(val)
      .addReg(state)
      .addImm(GetOffset(name))
      .addImm(AArch64_AM::getShiftValue(0)),
      sti_
  );
}

// -----------------------------------------------------------------------------
void AArch64RuntimePrinter::LoadState(
    llvm::Register state,
    llvm::Register val,
    const char *name)
{
  os_->emitInstruction(MCInstBuilder(AArch64::LDRXui)
      .addReg(val)
      .addReg(state)
      .addImm(GetOffset(name))
      .addImm(AArch64_AM::getShiftValue(0)),
      sti_
  );
}
