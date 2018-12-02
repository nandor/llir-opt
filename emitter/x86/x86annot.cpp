// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/CodeGen/MachineModuleInfo.h>
#include <llvm/MC/MCObjectFileInfo.h>
#include <llvm/CodeGen/LiveVariables.h>

#include "core/prog.h"
#include "core/func.h"
#include "core/block.h"
#include "emitter/x86/x86isel.h"
#include "emitter/x86/x86annot.h"



// -----------------------------------------------------------------------------
char X86Annot::ID;



// -----------------------------------------------------------------------------
X86Annot::X86Annot(
    const Prog *prog,
    const X86ISel *isel,
    llvm::MCContext *ctx,
    llvm::MCStreamer *os,
    const llvm::MCObjectFileInfo *objInfo)
  : llvm::ModulePass(ID)
  , prog_(prog)
  , isel_(isel)
  , ctx_(ctx)
  , os_(os)
  , objInfo_(objInfo)
{
}

// -----------------------------------------------------------------------------
bool X86Annot::runOnModule(llvm::Module &M)
{
  for (const auto &func : *prog_) {
    llvm::MachineFunction *MF = (*isel_)[&func];

    for (const auto &block : func) {
      for (const auto &inst : block) {
        if (inst.HasAnnotation(CAML_CALL_FRAME)) {
          LowerCallFrame(MF, &inst);
        }
        if (inst.HasAnnotation(CAML_RAISE_FRAME)) {
          LowerRaiseFrame(MF, &inst);
        }
      }
    }
  }

  os_->SwitchSection(objInfo_->getDataSection());
  os_->EmitLabel(ctx_->getOrCreateSymbol("genm_frametable"));
  os_->EmitIntValue(frames_.size(), 8);
  for (const auto &frame : frames_) {
    LowerFrame(frame);
  }

  return false;
}

// -----------------------------------------------------------------------------
void X86Annot::LowerCallFrame(llvm::MachineFunction *MF, const Inst *inst)
{
  auto &MRI = MF->getRegInfo();
  auto &MFI = MF->getFrameInfo();

  auto *func = inst->getParent()->getParent();
  llvm::MCSymbol *symbol = (*isel_)[inst];

  // Find the block containing the call frame.
  llvm::MachineBasicBlock *miBlock = nullptr;
  llvm::MachineInstr *miInst = nullptr;
  for (auto &MBB: *MF) {
    for (auto &MI : MBB) {
      if (!MI.isEHLabel()) {
        continue;
      }

      if (MI.getOperand(0).getMCSymbol() == symbol) {
        miBlock = &MBB;
        miInst = &MI;
        break;
      }
    }
  }

  // Check if we need to spill regs, in addition to registers.
  unsigned numObjs = MFI.getNumObjects();
  if (numObjs > 1 || (numObjs == 1 && func->GetStackSize() == 0)) {
    // Need to do LVA here to determine live stack slots.
    assert(!"not implemented");
  }

  // Add the list of live registers to the live set.
  if (!MRI.tracksLiveness()) {
    throw std::runtime_error("Missing liveness information!");
  }
}

// -----------------------------------------------------------------------------
void X86Annot::LowerRaiseFrame(llvm::MachineFunction *MF, const Inst *inst)
{
  auto &MFI = MF->getFrameInfo();

  FrameInfo frame;
  frame.Label = (*isel_)[inst];
  frame.FrameSize = MFI.getStackSize() + 8;
  frames_.push_back(frame);
}

// -----------------------------------------------------------------------------
void X86Annot::LowerFrame(const FrameInfo &info)
{
  os_->EmitSymbolValue(info.Label, 8);
  os_->EmitIntValue(info.FrameSize, 2);
  os_->EmitIntValue(info.Live.size(), 2);
  for (auto live : info.Live) {
    os_->EmitIntValue(live, 2);
  }
  os_->EmitValueToAlignment(8);
}

// -----------------------------------------------------------------------------
llvm::StringRef X86Annot::getPassName() const
{
  return "GenM X86 Annotation Inserter";
}

// -----------------------------------------------------------------------------
void X86Annot::getAnalysisUsage(llvm::AnalysisUsage &AU) const
{
  AU.addRequired<llvm::MachineModuleInfo>();
  AU.addPreserved<llvm::MachineModuleInfo>();
}
