// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/BinaryFormat/ELF.h>
#include <llvm/CodeGen/MachineModuleInfo.h>
#include <llvm/IR/Mangler.h>
#include <llvm/MC/MCSectionELF.h>
#include <llvm/Target/X86/X86InstrInfo.h>

#include "core/cast.h"
#include "core/data.h"
#include "core/prog.h"
#include "core/block.h"
#include "core/func.h"
#include "emitter/x86/x86runtime_printer.h"

using MCSymbol = llvm::MCSymbol;
using MCInst = llvm::MCInst;
using MCOperand = llvm::MCOperand;
namespace X86 = llvm::X86;



// -----------------------------------------------------------------------------
char X86RuntimePrinter::ID;

// -----------------------------------------------------------------------------
X86RuntimePrinter::X86RuntimePrinter(
    const Prog &prog,
    const llvm::TargetMachine &tm,
    llvm::MCContext &ctx,
    llvm::MCStreamer &os,
    const llvm::MCObjectFileInfo &objInfo,
    bool shared)
  : RuntimePrinter(ID, prog, tm, ctx, os, objInfo, shared)
{
}

// -----------------------------------------------------------------------------
llvm::StringRef X86RuntimePrinter::getPassName() const
{
  return "LLIR X86 Data Section Printer";
}

// -----------------------------------------------------------------------------
static std::vector<unsigned> kGPRegs{
  X86::RBP,
  X86::R11,
  X86::R10,
  X86::R13,
  X86::R12,
  X86::R9,
  X86::R8,
  X86::RCX,
  X86::RDX,
  X86::RSI,
  X86::RDI,
  X86::RBX,
  X86::RAX,
};

// -----------------------------------------------------------------------------
void X86RuntimePrinter::EmitCamlCallGc(llvm::Function &F)
{
  auto &sti = tm_.getSubtarget<llvm::X86Subtarget>(F);

  os_.SwitchSection(objInfo_.getTextSection());
  os_.emitCodeAlignment(16);
  os_.emitLabel(LowerSymbol("caml_call_gc"));

  // pushq %reg
  for (auto it = kGPRegs.begin(); it != kGPRegs.end(); ++it) {
    MCInst pushRBP;
    pushRBP.setOpcode(X86::PUSH64r);
    pushRBP.addOperand(MCOperand::createReg(*it));
    os_.emitInstruction(pushRBP, sti);
  }

  // movq    %rsp, gc_regs(%r14)
  LowerStore(X86::RSP, X86::R14, "gc_regs", sti);

  // movq    %r15, young_ptr(%r14)
  LowerStore(X86::R15, X86::R14, "young_ptr", sti);

  // movq    $$(%rsp), %rbp
  // movq    %rbp, last_return_address(%r14)
  {
    MCInst loadAddr;
    loadAddr.setOpcode(X86::MOV64rm);
    loadAddr.addOperand(MCOperand::createReg(X86::RBP));
    loadAddr.addOperand(MCOperand::createReg(X86::RSP));
    loadAddr.addOperand(MCOperand::createImm(1));
    loadAddr.addOperand(MCOperand::createReg(0));
    loadAddr.addOperand(MCOperand::createImm(kGPRegs.size() * 8 + 0));
    loadAddr.addOperand(MCOperand::createReg(0));
    os_.emitInstruction(loadAddr, sti);

    LowerStore(X86::RBP, X86::R14, "last_return_address", sti);
  }

  // leaq    $$(%rsp), %rbp
  // movq    %rbp, bottom_of_stack(%r14)
  {
    MCInst loadStk;
    loadStk.setOpcode(X86::LEA64r);
    loadStk.addOperand(MCOperand::createReg(X86::RBP));
    loadStk.addOperand(MCOperand::createReg(X86::RSP));
    loadStk.addOperand(MCOperand::createImm(1));
    loadStk.addOperand(MCOperand::createReg(0));
    loadStk.addOperand(MCOperand::createImm(kGPRegs.size() * 8 + 8));
    loadStk.addOperand(MCOperand::createReg(0));
    os_.emitInstruction(loadStk, sti);

    LowerStore(X86::RBP, X86::R14, "bottom_of_stack", sti);
  }

  // subq  $(16 * 8 * 4), %rsp
  {
    MCInst subStk;
    subStk.setOpcode(X86::SUB64ri32);
    subStk.addOperand(MCOperand::createReg(X86::RSP));
    subStk.addOperand(MCOperand::createReg(X86::RSP));
    subStk.addOperand(MCOperand::createImm(16 * 8 * 4));
    os_.emitInstruction(subStk, sti);
  }

  for (unsigned i = 0; i < 16; ++i) {
    MCInst saveXMM;
    saveXMM.setOpcode(X86::MOVAPSmr);
    saveXMM.addOperand(MCOperand::createReg(X86::RSP));
    saveXMM.addOperand(MCOperand::createImm(1));
    saveXMM.addOperand(MCOperand::createReg(0));
    saveXMM.addOperand(MCOperand::createImm(i * 32));
    saveXMM.addOperand(MCOperand::createReg(0));
    saveXMM.addOperand(MCOperand::createReg(X86::XMM0 + i));
    os_.emitInstruction(saveXMM, sti);
  }

  // callq caml_garbage_collection
  {
    MCInst call;
    call.setOpcode(X86::CALL64pcrel32);
    call.addOperand(LowerOperand("caml_garbage_collection"));
    os_.emitInstruction(call, sti);
  }

  for (unsigned i = 0; i < 16; ++i) {
    MCInst saveXMM;
    saveXMM.setOpcode(X86::MOVAPSrm);
    saveXMM.addOperand(MCOperand::createReg(X86::XMM0 + i));
    saveXMM.addOperand(MCOperand::createReg(X86::RSP));
    saveXMM.addOperand(MCOperand::createImm(1));
    saveXMM.addOperand(MCOperand::createReg(0));
    saveXMM.addOperand(MCOperand::createImm(i * 32));
    saveXMM.addOperand(MCOperand::createReg(0));
    os_.emitInstruction(saveXMM, sti);
  }

  // addq  $(16 * 8 * 4), %rsp
  MCInst addStk;
  addStk.setOpcode(X86::ADD64ri32);
  addStk.addOperand(MCOperand::createReg(X86::RSP));
  addStk.addOperand(MCOperand::createReg(X86::RSP));
  addStk.addOperand(MCOperand::createImm(16 * 8 * 4));
  os_.emitInstruction(addStk, sti);

  // popq %reg
  for (auto it = kGPRegs.rbegin(); it != kGPRegs.rend(); ++it) {
    MCInst pushRBP;
    pushRBP.setOpcode(X86::POP64r);
    pushRBP.addOperand(MCOperand::createReg(*it));
    os_.emitInstruction(pushRBP, sti);
  }

  // movq  young_ptr(%r14), %r15
  LowerLoad(X86::R15, X86::R14, "young_ptr", sti);

  // retq
  MCInst ret;
  ret.setOpcode(X86::RETQ);
  os_.emitInstruction(ret, sti);
}

// -----------------------------------------------------------------------------
void X86RuntimePrinter::EmitCamlCCall(llvm::Function &F)
{
  auto &sti = tm_.getSubtarget<llvm::X86Subtarget>(F);

  // caml_c_call:
  auto *sym = LowerSymbol("caml_c_call");
  os_.SwitchSection(objInfo_.getTextSection());
  os_.emitCodeAlignment(16);
  os_.emitLabel(sym);
  os_.emitSymbolAttribute(sym, llvm::MCSA_Global);

  // popq  %r10
  MCInst popR10;
  popR10.setOpcode(X86::POP64r);
  popR10.addOperand(MCOperand::createReg(X86::R10));
  os_.emitInstruction(popR10, sti);

  // movq    Caml_state(%rip), %r11
  LowerCamlState(X86::R11, sti);
  // movq  %r10, (Caml_state->last_return_address)(%rip)
  LowerStore(X86::R10, X86::R11, "last_return_address", sti);
  // movq  %rsp, (Caml_state->bottom_of_stack)(%rip)
  LowerStore(X86::RSP, X86::R11, "bottom_of_stack", sti);

  // pushq %r10
  MCInst pushR10;
  pushR10.setOpcode(X86::PUSH64r);
  pushR10.addOperand(MCOperand::createReg(X86::R10));
  os_.emitInstruction(pushR10, sti);

  // jmpq  *%rax
  MCInst jmpRAX;
  jmpRAX.setOpcode(X86::JMP64r);
  jmpRAX.addOperand(MCOperand::createReg(X86::RAX));
  os_.emitInstruction(jmpRAX, sti);
}

// -----------------------------------------------------------------------------
MCSymbol *X86RuntimePrinter::LowerSymbol(const std::string &name)
{
  llvm::SmallString<128> sym;
  llvm::Mangler::getNameWithPrefix(sym, name.data(), layout_);
  return ctx_.getOrCreateSymbol(sym);
}

// -----------------------------------------------------------------------------
MCOperand X86RuntimePrinter::LowerOperand(const std::string &name, unsigned offset)
{
  return LowerOperand(LowerSymbol(name), offset);
}

// -----------------------------------------------------------------------------
MCOperand X86RuntimePrinter::LowerOperand(MCSymbol *symbol, unsigned offset)
{
  auto *symExpr = llvm::MCSymbolRefExpr::create(symbol, ctx_);
  if (offset == 0) {
    return llvm::MCOperand::createExpr(symExpr);
  } else {
    return llvm::MCOperand::createExpr(llvm::MCBinaryExpr::createAdd(
      symExpr,
      llvm::MCConstantExpr::create(offset, ctx_),
      ctx_
    ));
  }
}

// -----------------------------------------------------------------------------
void X86RuntimePrinter::LowerCamlState(
    unsigned reg,
    const llvm::X86Subtarget &sti)
{
  MCInst addrInst;
  addrInst.setOpcode(X86::MOV64rm);
  addrInst.addOperand(MCOperand::createReg(reg));
  addrInst.addOperand(MCOperand::createReg(X86::RIP));
  addrInst.addOperand(MCOperand::createImm(1));
  addrInst.addOperand(MCOperand::createReg(0));
  addrInst.addOperand(LowerOperand("Caml_state"));
  addrInst.addOperand(MCOperand::createReg(0));
  os_.emitInstruction(addrInst, sti);
}

// -----------------------------------------------------------------------------
void X86RuntimePrinter::LowerStore(
    unsigned reg,
    unsigned state,
    const std::string &name,
    const llvm::X86Subtarget &sti)
{
  MCInst inst;
  inst.setOpcode(X86::MOV64mr);
  AddAddr(inst, state, name);
  inst.addOperand(MCOperand::createReg(reg));
  os_.emitInstruction(inst, sti);
}

// -----------------------------------------------------------------------------
void X86RuntimePrinter::LowerLoad(
    unsigned reg,
    unsigned state,
    const std::string &name,
    const llvm::X86Subtarget &sti)
{
  MCInst inst;
  inst.setOpcode(X86::MOV64rm);
  inst.addOperand(MCOperand::createReg(reg));
  AddAddr(inst, state, name);
  os_.emitInstruction(inst, sti);
}

// -----------------------------------------------------------------------------
static const std::unordered_map<std::string, unsigned> kOffsets =
{
  #define FIELD(name, index) { #name, index },
  #include "core/state.h"
  #undef FIELD
};

// -----------------------------------------------------------------------------
void X86RuntimePrinter::AddAddr(MCInst &MI, unsigned reg, const std::string &name)
{
  auto it = kOffsets.find(name);
  assert(it != kOffsets.end() && "missing offset");

  MI.addOperand(MCOperand::createReg(reg));
  MI.addOperand(MCOperand::createImm(1));
  MI.addOperand(MCOperand::createReg(0));
  MI.addOperand(MCOperand::createImm(it->second * 8));
  MI.addOperand(MCOperand::createReg(0));
}
