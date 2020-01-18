// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/BinaryFormat/ELF.h>
#include <llvm/CodeGen/MachineModuleInfo.h>
#include <llvm/IR/Mangler.h>
#include <llvm/MC/MCSectionELF.h>
#include <llvm/Target/X86/X86InstrInfo.h>

#include "core/data.h"
#include "core/prog.h"
#include "core/block.h"
#include "emitter/isel.h"
#include "emitter/x86/x86runtime.h"

using MCSymbol = llvm::MCSymbol;
using MCInst = llvm::MCInst;
using MCOperand = llvm::MCOperand;
namespace X86 = llvm::X86;

// -----------------------------------------------------------------------------
char X86Runtime::ID;



// -----------------------------------------------------------------------------
X86Runtime::X86Runtime(
    const Prog *prog,
    llvm::MCContext *ctx,
    llvm::MCStreamer *os,
    const llvm::MCObjectFileInfo *objInfo,
    const llvm::DataLayout &layout,
    const llvm::X86Subtarget &sti)
  : llvm::ModulePass(ID)
  , prog_(prog)
  , ctx_(ctx)
  , os_(os)
  , objInfo_(objInfo)
  , layout_(layout)
  , sti_(sti)
{
}

// -----------------------------------------------------------------------------
bool X86Runtime::runOnModule(llvm::Module &)
{
  EmitCamlCallGc();
  EmitCamlCCall();
  EmitCamlAlloc(1);
  EmitCamlAlloc(2);
  EmitCamlAlloc(3);
  EmitCamlAlloc({});
  return false;
}

// -----------------------------------------------------------------------------
llvm::StringRef X86Runtime::getPassName() const
{
  return "GenM Data Section Printer";
}

// -----------------------------------------------------------------------------
void X86Runtime::getAnalysisUsage(llvm::AnalysisUsage &AU) const
{
  AU.setPreservesAll();
  AU.addRequired<llvm::MachineModuleInfo>();
}

// -----------------------------------------------------------------------------
static std::vector<unsigned> kGPRegs{
  X86::R15,
  X86::R14,
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
void X86Runtime::EmitCamlCallGc()
{
  os_->SwitchSection(objInfo_->getTextSection());
  os_->EmitLabel(LowerSymbol("caml_call_gc"));

  // pushq %reg
  for (auto it = kGPRegs.begin(); it != kGPRegs.end(); ++it) {
    MCInst pushRBP;
    pushRBP.setOpcode(X86::PUSH64r);
    pushRBP.addOperand(MCOperand::createReg(*it));
    os_->EmitInstruction(pushRBP, sti_);
  }

  // movq    %rsp, caml_gc_regs(%rip)
  LowerStore(X86::RSP, "caml_gc_regs");

  // movq    $$(%rsp), %rax
  MCInst loadAddr;
  loadAddr.setOpcode(X86::MOV64rm);
  loadAddr.addOperand(MCOperand::createReg(X86::RAX));
  loadAddr.addOperand(MCOperand::createReg(X86::RSP));
  loadAddr.addOperand(MCOperand::createImm(0));
  loadAddr.addOperand(MCOperand::createReg(0));
  loadAddr.addOperand(MCOperand::createImm(15 * 8 + 8 + 8));
  loadAddr.addOperand(MCOperand::createReg(0));
  os_->EmitInstruction(loadAddr, sti_);

  // movq  %rbp, caml_last_return_address(%rip)
  LowerStore(X86::RAX, "caml_last_return_address");

  // leaq    $$(%rsp), %rax
  MCInst loadStk;
  loadStk.setOpcode(X86::LEA64r);
  loadStk.addOperand(MCOperand::createReg(X86::RAX));
  loadStk.addOperand(MCOperand::createReg(X86::RSP));
  loadStk.addOperand(MCOperand::createImm(0));
  loadStk.addOperand(MCOperand::createReg(0));
  loadStk.addOperand(MCOperand::createImm(15 * 8 + 8 + 16));
  loadStk.addOperand(MCOperand::createReg(0));
  os_->EmitInstruction(loadStk, sti_);

  // movq  %rax, caml_bottom_of_stack(%rip)
  LowerStore(X86::RAX, "caml_bottom_of_stack");

  // subq  $(16 * 8 * 4), %rsp
  MCInst subStk;
  subStk.setOpcode(X86::SUB64ri32);
  subStk.addOperand(MCOperand::createReg(X86::RSP));
  subStk.addOperand(MCOperand::createReg(X86::RSP));
  subStk.addOperand(MCOperand::createImm(16 * 8 * 4));
  os_->EmitInstruction(subStk, sti_);

  for (unsigned i = 0; i < 16; ++i) {
    MCInst saveXMM;
    saveXMM.setOpcode(X86::VMOVAPSmr);
    saveXMM.addOperand(MCOperand::createReg(X86::RSP));
    saveXMM.addOperand(MCOperand::createImm(0));
    saveXMM.addOperand(MCOperand::createReg(0));
    saveXMM.addOperand(MCOperand::createImm(i * 32));
    saveXMM.addOperand(MCOperand::createReg(0));
    saveXMM.addOperand(MCOperand::createReg(X86::XMM0 + i));
    os_->EmitInstruction(saveXMM, sti_);
  }

  // callq caml_garbage_collection
  MCInst call;
  call.setOpcode(X86::CALL64pcrel32);
  call.addOperand(LowerOperand("caml_garbage_collection"));
  os_->EmitInstruction(call, sti_);

  for (unsigned i = 0; i < 16; ++i) {
    MCInst saveXMM;
    saveXMM.setOpcode(X86::VMOVAPSrm);
    saveXMM.addOperand(MCOperand::createReg(X86::XMM0 + i));
    saveXMM.addOperand(MCOperand::createReg(X86::RSP));
    saveXMM.addOperand(MCOperand::createImm(0));
    saveXMM.addOperand(MCOperand::createReg(0));
    saveXMM.addOperand(MCOperand::createImm(i * 32));
    saveXMM.addOperand(MCOperand::createReg(0));
    os_->EmitInstruction(saveXMM, sti_);
  }

  // addq  $(16 * 8 * 4), %rsp
  MCInst addStk;
  addStk.setOpcode(X86::ADD64ri32);
  addStk.addOperand(MCOperand::createReg(X86::RSP));
  addStk.addOperand(MCOperand::createReg(X86::RSP));
  addStk.addOperand(MCOperand::createImm(16 * 8 * 4));
  os_->EmitInstruction(addStk, sti_);

  // popq %reg
  for (auto it = kGPRegs.rbegin(); it != kGPRegs.rend(); ++it) {
    MCInst pushRBP;
    pushRBP.setOpcode(X86::POP64r);
    pushRBP.addOperand(MCOperand::createReg(*it));
    os_->EmitInstruction(pushRBP, sti_);
  }

  // retq
  MCInst ret;
  ret.setOpcode(X86::RETQ);
  os_->EmitInstruction(ret, sti_);
}

// -----------------------------------------------------------------------------
void X86Runtime::EmitCamlCCall()
{
  // caml_c_call:
  os_->SwitchSection(objInfo_->getTextSection());
  os_->EmitLabel(LowerSymbol("caml_c_call"));

  // popq  %rbp
  MCInst popRBP;
  popRBP.setOpcode(X86::POP64r);
  popRBP.addOperand(MCOperand::createReg(X86::RBP));
  os_->EmitInstruction(popRBP, sti_);

  // movq  %rbp, caml_last_return_address(%rip)
  LowerStore(X86::RBP, "caml_last_return_address");
  // movq  %rsp, caml_bottom_of_stack(%rip)
  LowerStore(X86::RSP, "caml_bottom_of_stack");

  // pushq %rbp
  MCInst pushRBP;
  pushRBP.setOpcode(X86::PUSH64r);
  pushRBP.addOperand(MCOperand::createReg(X86::RBP));
  os_->EmitInstruction(pushRBP, sti_);

  // jmpq  *%rax
  MCInst jmpRAX;
  jmpRAX.setOpcode(X86::JMP64r);
  jmpRAX.addOperand(MCOperand::createReg(X86::RAX));
  os_->EmitInstruction(jmpRAX, sti_);
}

// -----------------------------------------------------------------------------
void X86Runtime::EmitCamlAlloc(const std::optional<unsigned> N)
{
  os_->SwitchSection(objInfo_->getTextSection());
  auto *branchRestart = ctx_->createTempSymbol();
  auto *branchCollect = ctx_->createTempSymbol();
  if (N) {
    os_->EmitLabel(LowerSymbol("caml_alloc" + std::to_string(*N)));
    os_->EmitLabel(branchRestart);

    // subq  $n * 8 + 8, caml_young_ptr(%rip)
    MCInst sub;
    sub.setOpcode(X86::SUB64mi8);
    AddAddr(sub, "caml_young_ptr");
    sub.addOperand(MCOperand::createImm(8 + *N * 8));
    os_->EmitInstruction(sub, sti_);
  } else {
    os_->EmitLabel(LowerSymbol("caml_allocN"));

    // pushq %rax
    MCInst pushRAX;
    pushRAX.setOpcode(X86::PUSH64r);
    pushRAX.addOperand(MCOperand::createReg(X86::RAX));
    os_->EmitInstruction(pushRAX, sti_);

    os_->EmitLabel(branchRestart);

    // subq  %rax, caml_young_ptr(%rip)
    MCInst sub;
    sub.setOpcode(X86::SUB64mr);
    AddAddr(sub, "caml_young_ptr");
    sub.addOperand(MCOperand::createReg(X86::RAX));
    os_->EmitInstruction(sub, sti_);
  }

  // movq  caml_young_ptr(%rip), %rax
  LowerLoad(X86::RAX, "caml_young_ptr");

  // cmpq  caml_young_limit(%rip), %rax
  MCInst cmp;
  cmp.setOpcode(X86::CMP64rm);
  cmp.addOperand(MCOperand::createReg(X86::RAX));
  AddAddr(cmp, "caml_young_limit");
  os_->EmitInstruction(cmp, sti_);

  // jb  .Lcollect
  MCInst jb;
  jb.setOpcode(X86::JB_1);
  jb.addOperand(MCOperand::createExpr(llvm::MCSymbolRefExpr::create(
      branchCollect,
      *ctx_
  )));
  os_->EmitInstruction(jb, sti_);

  if (!N) {
    // addq  $8, %rsp
    MCInst add;
    add.setOpcode(X86::ADD64ri8);
    add.addOperand(MCOperand::createReg(X86::RSP));
    add.addOperand(MCOperand::createReg(X86::RSP));
    add.addOperand(MCOperand::createImm(8));
    os_->EmitInstruction(add, sti_);
  }

  // addq  $8, %rax
  MCInst add;
  add.setOpcode(X86::ADD64ri8);
  add.addOperand(MCOperand::createReg(X86::RAX));
  add.addOperand(MCOperand::createReg(X86::RAX));
  add.addOperand(MCOperand::createImm(8));
  os_->EmitInstruction(add, sti_);

  // retq
  MCInst ret;
  ret.setOpcode(X86::RETQ);
  os_->EmitInstruction(ret, sti_);

  os_->EmitLabel(branchCollect);

  if (N) {
    // addq  $n * 8 + 8, caml_young_ptr(%rip)
    MCInst add;
    add.setOpcode(X86::ADD64mi8);
    AddAddr(add, "caml_young_ptr");
    add.addOperand(MCOperand::createImm(8 + *N * 8));
    os_->EmitInstruction(add, sti_);

    // subq  $8, %rsp
    MCInst push;
    push.setOpcode(X86::SUB64ri8);
    push.addOperand(MCOperand::createReg(X86::RSP));
    push.addOperand(MCOperand::createReg(X86::RSP));
    push.addOperand(MCOperand::createImm(8));
    os_->EmitInstruction(push, sti_);
  } else {
    MCInst movRAX;
    movRAX.setOpcode(X86::MOV64rm);
    movRAX.addOperand(MCOperand::createReg(X86::RAX));
    movRAX.addOperand(MCOperand::createReg(X86::RSP));
    movRAX.addOperand(MCOperand::createImm(1));
    movRAX.addOperand(MCOperand::createReg(0));
    movRAX.addOperand(MCOperand::createImm(0));
    movRAX.addOperand(MCOperand::createReg(0));
    os_->EmitInstruction(movRAX, sti_);

    MCInst sub;
    sub.setOpcode(X86::ADD64mr);
    AddAddr(sub, "caml_young_ptr");
    sub.addOperand(MCOperand::createReg(X86::RAX));
    os_->EmitInstruction(sub, sti_);
  }

  // callq caml_call_gc
  MCInst call;
  call.setOpcode(X86::CALL64pcrel32);
  call.addOperand(LowerOperand("caml_call_gc"));
  os_->EmitInstruction(call, sti_);

  if (N) {
    // addq  $8, %rsp
    MCInst pop;
    pop.setOpcode(X86::ADD64ri8);
    pop.addOperand(MCOperand::createReg(X86::RSP));
    pop.addOperand(MCOperand::createReg(X86::RSP));
    pop.addOperand(MCOperand::createImm(8));
    os_->EmitInstruction(pop, sti_);
  }

  // jmp  .Lrestart
  MCInst jmp;
  jmp.setOpcode(X86::JMP_1);
  jmp.addOperand(MCOperand::createExpr(llvm::MCSymbolRefExpr::create(
      branchRestart,
      *ctx_
  )));
  os_->EmitInstruction(jmp, sti_);
}

// -----------------------------------------------------------------------------
MCSymbol *X86Runtime::LowerSymbol(const std::string_view name)
{
  llvm::SmallString<128> sym;
  llvm::Mangler::getNameWithPrefix(sym, name.data(), layout_);
  return ctx_->getOrCreateSymbol(sym);
}

// -----------------------------------------------------------------------------
MCOperand X86Runtime::LowerOperand(const std::string_view name)
{
  return MCOperand::createExpr(llvm::MCSymbolRefExpr::create(
      LowerSymbol(name),
      *ctx_
  ));
}

// -----------------------------------------------------------------------------
void X86Runtime::LowerStore(unsigned Reg, const std::string_view name)
{
  MCInst inst;
  inst.setOpcode(X86::MOV64mr);
  AddAddr(inst, name);
  inst.addOperand(MCOperand::createReg(Reg));
  os_->EmitInstruction(inst, sti_);
}

// -----------------------------------------------------------------------------
void X86Runtime::LowerLoad(unsigned Reg, const std::string_view name)
{
  MCInst inst;
  inst.setOpcode(X86::MOV64rm);
  inst.addOperand(MCOperand::createReg(Reg));
  AddAddr(inst, name);
  os_->EmitInstruction(inst, sti_);
}

// -----------------------------------------------------------------------------
void X86Runtime::AddAddr(MCInst &MI, const std::string_view name)
{
  MI.addOperand(MCOperand::createReg(X86::RIP));
  MI.addOperand(MCOperand::createImm(1));
  MI.addOperand(MCOperand::createReg(0));
  MI.addOperand(LowerOperand(name));
  MI.addOperand(MCOperand::createReg(0));
}
