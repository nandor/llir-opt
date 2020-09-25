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
#include "emitter/x86/x86runtime.h"

using MCSymbol = llvm::MCSymbol;
using MCInst = llvm::MCInst;
using MCOperand = llvm::MCOperand;
namespace X86 = llvm::X86;



// -----------------------------------------------------------------------------
char X86Runtime::ID;

// -----------------------------------------------------------------------------
X86Runtime::X86Runtime(
    const Prog &prog,
    llvm::MCContext *ctx,
    llvm::MCStreamer *os,
    const llvm::MCObjectFileInfo *objInfo,
    const llvm::DataLayout &layout,
    const llvm::X86Subtarget &sti,
    bool shared)
  : llvm::ModulePass(ID)
  , prog_(prog)
  , ctx_(ctx)
  , os_(os)
  , objInfo_(objInfo)
  , layout_(layout)
  , sti_(sti)
  , shared_(shared)
{
}

// -----------------------------------------------------------------------------
bool X86Runtime::runOnModule(llvm::Module &)
{
  // Emit the OCaml runtime components.
  if (!shared_) {
    {
      bool needsCallGC = false;
      for (auto &ext : prog_.externs()) {
        if (ext.getName() == "caml_alloc1") {
          EmitCamlAlloc(1);
          needsCallGC = true;
          continue;
        }
        if (ext.getName() == "caml_alloc2") {
          EmitCamlAlloc(2);
          needsCallGC = true;
          continue;
        }
        if (ext.getName() == "caml_alloc3") {
          EmitCamlAlloc(3);
          needsCallGC = true;
          continue;
        }
        if (ext.getName() == "caml_allocN") {
          EmitCamlAlloc({});
          needsCallGC = true;
          continue;
        }
      }
      if (needsCallGC) {
        EmitCamlCallGc();
      }
    }

    {
      bool needsCCall = false;
      for (auto &func : prog_) {
        if (func.GetCallingConv() != CallingConv::CAML) {
          for (auto *user : func.users()) {
            if (auto *movInst = ::dyn_cast_or_null<const MovInst>(user)) {
              auto *caller = movInst->getParent()->getParent();
              if (caller->GetCallingConv() == CallingConv::CAML) {
                needsCCall = true;
                break;
              }
            }
          }
        }
      }
      if (needsCCall) {
        EmitCamlCCall();
      }
    }
  }
  return false;
}

// -----------------------------------------------------------------------------
llvm::StringRef X86Runtime::getPassName() const
{
  return "LLIR Data Section Printer";
}

// -----------------------------------------------------------------------------
void X86Runtime::getAnalysisUsage(llvm::AnalysisUsage &AU) const
{
  AU.setPreservesAll();
  AU.addRequired<llvm::MachineModuleInfoWrapperPass>();
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
void X86Runtime::EmitStart()
{
  os_->SwitchSection(objInfo_->getTextSection());
  os_->emitCodeAlignment(16);
  auto *start = LowerSymbol("_start");
  os_->emitLabel(start);
  os_->emitSymbolAttribute(start, llvm::MCSA_Global);

  // xorq %rbp, %rbp
  MCInst xorRBP;
  xorRBP.setOpcode(X86::XOR64rr);
  xorRBP.addOperand(MCOperand::createReg(X86::RBP));
  xorRBP.addOperand(MCOperand::createReg(X86::RBP));
  xorRBP.addOperand(MCOperand::createReg(X86::RBP));
  os_->emitInstruction(xorRBP, sti_);

  // xorq %rsp, %rdi
  MCInst movRSP;
  movRSP.setOpcode(X86::MOV64rr);
  movRSP.addOperand(MCOperand::createReg(X86::RDI));
  movRSP.addOperand(MCOperand::createReg(X86::RSP));
  os_->emitInstruction(movRSP, sti_);

  // lea _DYNAMIC(%rip), %rsi
  if (shared_) {
    auto *dynamic = LowerSymbol("_DYNAMIC");
    os_->emitSymbolAttribute(dynamic, llvm::MCSA_Hidden);
    os_->emitSymbolAttribute(dynamic, llvm::MCSA_Weak);

    MCInst loadStk;
    loadStk.setOpcode(X86::LEA64r);
    loadStk.addOperand(MCOperand::createReg(X86::RSI));
    loadStk.addOperand(MCOperand::createReg(X86::RIP));
    loadStk.addOperand(MCOperand::createImm(1));
    loadStk.addOperand(MCOperand::createReg(0));
    loadStk.addOperand(LowerOperand(dynamic));
    loadStk.addOperand(MCOperand::createReg(0));
    os_->emitInstruction(loadStk, sti_);
  } else {
    // xorq %rsi, %rsi
    MCInst xorRSI;
    xorRSI.setOpcode(X86::XOR64rr);
    xorRSI.addOperand(MCOperand::createReg(X86::RSI));
    xorRSI.addOperand(MCOperand::createReg(X86::RSI));
    xorRSI.addOperand(MCOperand::createReg(X86::RSI));
    os_->emitInstruction(xorRSI, sti_);
  }

  // andq $-16, %rsp
  MCInst subStk;
  subStk.setOpcode(X86::AND64ri32);
  subStk.addOperand(MCOperand::createReg(X86::RSP));
  subStk.addOperand(MCOperand::createReg(X86::RSP));
  subStk.addOperand(MCOperand::createImm(-16));
  os_->emitInstruction(subStk, sti_);

  // callq start_c
  MCInst call;
  call.setOpcode(X86::CALL64pcrel32);
  call.addOperand(LowerOperand("_start_c"));
  os_->emitInstruction(call, sti_);
}

// -----------------------------------------------------------------------------
void X86Runtime::EmitCamlCallGc()
{
  os_->SwitchSection(objInfo_->getTextSection());
  os_->emitCodeAlignment(16);
  os_->emitLabel(LowerSymbol("caml_call_gc"));

  // pushq %reg
  for (auto it = kGPRegs.begin(); it != kGPRegs.end(); ++it) {
    MCInst pushRBP;
    pushRBP.setOpcode(X86::PUSH64r);
    pushRBP.addOperand(MCOperand::createReg(*it));
    os_->emitInstruction(pushRBP, sti_);
  }

  // movq    Caml_state(%rip), %r14
  LowerCamlState(X86::R14);

  // movq    %rsp, gc_regs(%r14)
  LowerStore(X86::RSP, X86::R14, "gc_regs");

  // movq    $$(%rsp), %rbp
  // movq    %rbp, last_return_address(%r14)
  {
    MCInst loadAddr;
    loadAddr.setOpcode(X86::MOV64rm);
    loadAddr.addOperand(MCOperand::createReg(X86::RBP));
    loadAddr.addOperand(MCOperand::createReg(X86::RSP));
    loadAddr.addOperand(MCOperand::createImm(1));
    loadAddr.addOperand(MCOperand::createReg(0));
    loadAddr.addOperand(MCOperand::createImm(15 * 8 + 0));
    loadAddr.addOperand(MCOperand::createReg(0));
    os_->emitInstruction(loadAddr, sti_);

    LowerStore(X86::RBP, X86::R14, "last_return_address");
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
    loadStk.addOperand(MCOperand::createImm(15 * 8 + 8));
    loadStk.addOperand(MCOperand::createReg(0));
    os_->emitInstruction(loadStk, sti_);

    LowerStore(X86::RBP, X86::R14, "bottom_of_stack");
  }

  // subq  $(16 * 8 * 4), %rsp
  {
    MCInst subStk;
    subStk.setOpcode(X86::SUB64ri32);
    subStk.addOperand(MCOperand::createReg(X86::RSP));
    subStk.addOperand(MCOperand::createReg(X86::RSP));
    subStk.addOperand(MCOperand::createImm(16 * 8 * 4));
    os_->emitInstruction(subStk, sti_);
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
    os_->emitInstruction(saveXMM, sti_);
  }

  // callq caml_garbage_collection
  {
    MCInst call;
    call.setOpcode(X86::CALL64pcrel32);
    call.addOperand(LowerOperand("caml_garbage_collection"));
    os_->emitInstruction(call, sti_);
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
    os_->emitInstruction(saveXMM, sti_);
  }

  // addq  $(16 * 8 * 4), %rsp
  MCInst addStk;
  addStk.setOpcode(X86::ADD64ri32);
  addStk.addOperand(MCOperand::createReg(X86::RSP));
  addStk.addOperand(MCOperand::createReg(X86::RSP));
  addStk.addOperand(MCOperand::createImm(16 * 8 * 4));
  os_->emitInstruction(addStk, sti_);

  // popq %reg
  for (auto it = kGPRegs.rbegin(); it != kGPRegs.rend(); ++it) {
    MCInst pushRBP;
    pushRBP.setOpcode(X86::POP64r);
    pushRBP.addOperand(MCOperand::createReg(*it));
    os_->emitInstruction(pushRBP, sti_);
  }

  // movq    Caml_state(%rip), %r14
  LowerCamlState(X86::R14);

  // movq  young_ptr(%r14), %rax
  LowerLoad(X86::RAX, X86::R14, "young_ptr");

  // retq
  MCInst ret;
  ret.setOpcode(X86::RETQ);
  os_->emitInstruction(ret, sti_);
}

// -----------------------------------------------------------------------------
void X86Runtime::EmitCamlCCall()
{
  // caml_c_call:
  auto *sym = LowerSymbol("caml_c_call");
  os_->SwitchSection(objInfo_->getTextSection());
  os_->emitCodeAlignment(16);
  os_->emitLabel(sym);
  os_->emitSymbolAttribute(sym, llvm::MCSA_Global);

  // popq  %r10
  MCInst popR10;
  popR10.setOpcode(X86::POP64r);
  popR10.addOperand(MCOperand::createReg(X86::R10));
  os_->emitInstruction(popR10, sti_);

  // movq    Caml_state(%rip), %r11
  LowerCamlState(X86::R11);
  // movq  %r10, (Caml_state->last_return_address)(%rip)
  LowerStore(X86::R10, X86::R11, "last_return_address");
  // movq  %rsp, (Caml_state->bottom_of_stack)(%rip)
  LowerStore(X86::RSP, X86::R11, "bottom_of_stack");

  // pushq %r10
  MCInst pushR10;
  pushR10.setOpcode(X86::PUSH64r);
  pushR10.addOperand(MCOperand::createReg(X86::R10));
  os_->emitInstruction(pushR10, sti_);

  // jmpq  *%rax
  MCInst jmpRAX;
  jmpRAX.setOpcode(X86::JMP64r);
  jmpRAX.addOperand(MCOperand::createReg(X86::RAX));
  os_->emitInstruction(jmpRAX, sti_);
}

// -----------------------------------------------------------------------------
void X86Runtime::EmitCamlAlloc(const std::optional<unsigned> N)
{
  os_->SwitchSection(objInfo_->getTextSection());
  os_->emitCodeAlignment(16);

  if (N) {
    auto *sym = LowerSymbol("caml_alloc" + std::to_string(*N));
    os_->emitSymbolAttribute(sym, llvm::MCSA_Global);
    os_->emitLabel(sym);

    // movq    Caml_state(%rip), %r14
    LowerCamlState(X86::R14);

    // subq  $n * 8 + 8, young_ptr(%r14)
    MCInst sub;
    sub.setOpcode(X86::SUB64mi8);
    AddAddr(sub, X86::R14, "young_ptr");
    sub.addOperand(MCOperand::createImm(8 + *N * 8));
    os_->emitInstruction(sub, sti_);
  } else {
    auto *sym = LowerSymbol("caml_allocN");
    os_->emitSymbolAttribute(sym, llvm::MCSA_Global);
    os_->emitLabel(sym);

    // movq    Caml_state(%rip), %r14
    LowerCamlState(X86::R14);

    // subq  %rax, young_ptr(%r14)
    MCInst sub;
    sub.setOpcode(X86::SUB64mr);
    AddAddr(sub, X86::R14, "young_ptr");
    sub.addOperand(MCOperand::createReg(X86::RAX));
    os_->emitInstruction(sub, sti_);
  }

  // movq  young_ptr(%r14), %rax
  LowerLoad(X86::RAX, X86::R14, "young_ptr");

  // cmpq  young_limit(%r14), %rax
  MCInst cmp;
  cmp.setOpcode(X86::CMP64rm);
  cmp.addOperand(MCOperand::createReg(X86::RAX));
  AddAddr(cmp, X86::R14, "young_limit");
  os_->emitInstruction(cmp, sti_);

  // jb  .Lcollect
  MCInst jb;
  jb.setOpcode(X86::JCC_1);
  jb.addOperand(LowerOperand("caml_call_gc"));
  jb.addOperand(MCOperand::createImm(2));
  os_->emitInstruction(jb, sti_);

  // retq
  MCInst ret;
  ret.setOpcode(X86::RETQ);
  os_->emitInstruction(ret, sti_);
}

// -----------------------------------------------------------------------------
MCSymbol *X86Runtime::LowerSymbol(const std::string &name)
{
  llvm::SmallString<128> sym;
  llvm::Mangler::getNameWithPrefix(sym, name.data(), layout_);
  return ctx_->getOrCreateSymbol(sym);
}

// -----------------------------------------------------------------------------
MCOperand X86Runtime::LowerOperand(const std::string &name, unsigned offset)
{
  return LowerOperand(LowerSymbol(name), offset);
}

// -----------------------------------------------------------------------------
MCOperand X86Runtime::LowerOperand(MCSymbol *symbol, unsigned offset)
{
  auto *symExpr = llvm::MCSymbolRefExpr::create(symbol, *ctx_);
  if (offset == 0) {
    return llvm::MCOperand::createExpr(symExpr);
  } else {
    return llvm::MCOperand::createExpr(llvm::MCBinaryExpr::createAdd(
      symExpr,
      llvm::MCConstantExpr::create(offset, *ctx_),
      *ctx_
    ));
  }
}

// -----------------------------------------------------------------------------
void X86Runtime::LowerCamlState(unsigned reg)
{
  MCInst addrInst;
  addrInst.setOpcode(X86::MOV64rm);
  addrInst.addOperand(MCOperand::createReg(reg));
  addrInst.addOperand(MCOperand::createReg(X86::RIP));
  addrInst.addOperand(MCOperand::createImm(1));
  addrInst.addOperand(MCOperand::createReg(0));
  addrInst.addOperand(LowerOperand("Caml_state"));
  addrInst.addOperand(MCOperand::createReg(0));
  os_->emitInstruction(addrInst, sti_);
}

// -----------------------------------------------------------------------------
void X86Runtime::LowerStore(
    unsigned reg,
    unsigned state,
    const std::string &name)
{
  MCInst inst;
  inst.setOpcode(X86::MOV64mr);
  AddAddr(inst, state, name);
  inst.addOperand(MCOperand::createReg(reg));
  os_->emitInstruction(inst, sti_);
}

// -----------------------------------------------------------------------------
void X86Runtime::LowerLoad(
    unsigned reg,
    unsigned state,
    const std::string &name)
{
  MCInst inst;
  inst.setOpcode(X86::MOV64rm);
  inst.addOperand(MCOperand::createReg(reg));
  AddAddr(inst, state, name);
  os_->emitInstruction(inst, sti_);
}

// -----------------------------------------------------------------------------
static const std::unordered_map<std::string, unsigned> kOffsets =
{
  #define FIELD(name, index) { #name, index },
  #include "core/state.h"
  #undef FIELD
};

// -----------------------------------------------------------------------------
void X86Runtime::AddAddr(MCInst &MI, unsigned reg, const std::string &name)
{
  auto it = kOffsets.find(name);
  assert(it != kOffsets.end() && "missing offset");

  MI.addOperand(MCOperand::createReg(reg));
  MI.addOperand(MCOperand::createImm(1));
  MI.addOperand(MCOperand::createReg(0));
  MI.addOperand(MCOperand::createImm(it->second * 8));
  MI.addOperand(MCOperand::createReg(0));
}
