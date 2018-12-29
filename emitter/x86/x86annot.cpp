// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/BitVector.h>
#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/CodeGen/LiveVariables.h>
#include <llvm/CodeGen/MachineModuleInfo.h>
#include <llvm/MC/MCObjectFileInfo.h>

#include "core/block.h"
#include "core/cfg.h"
#include "core/func.h"
#include "core/prog.h"
#include "emitter/x86/x86annot.h"
#include "emitter/x86/x86isel.h"

namespace X86 = llvm::X86;
using MachineBasicBlock = llvm::MachineBasicBlock;
using MachineFunction = llvm::MachineFunction;
using MachineInstr = llvm::MachineInstr;



// -----------------------------------------------------------------------------
class X86LVA final {
private:
  using StackVal = llvm::FixedStackPseudoSourceValue;

public:
  /// Initialises the live variable info, performing per-block analysis.
  X86LVA(const Func *func, MachineFunction *MF)
  {
    auto &MRI = MF->getRegInfo();
    auto &MFI = MF->getFrameInfo();

    // Ensure liveness information was computed for all blocks.
    if (!MRI.tracksLiveness()) {
      throw std::runtime_error("Missing liveness information!");
    }

    // Initialise bit vectors for live regs.
    for (const auto &MBB : *MF) {
      LiveInfo &live = live_[&MBB];
      live.RegOut.resize(kNumRegs);
      for (const auto &SuccMBB : MBB.successors()) {
        for (const auto &reg : SuccMBB->liveins()) {
          if (auto physReg = regIndex(reg.PhysReg)) {
            live.RegOut[*physReg] = true;
          }
        }
      }
    }

    // Check if we need to spill regs, in addition to registers.
    unsigned numObjs = MFI.getNumObjects();
    if (numObjs > 1 || (numObjs == 1 && func->GetStackSize() == 0)) {
      firstSlot_ = func->GetStackSize() ? 1 : 0;
      numSlots_ = numObjs;

      // Reset the live variable info.
      for (const auto &MBB : *MF) {
        LiveInfo &live = live_[&MBB];
        live.SlotIn.resize(numSlots_);
        live.SlotOut.resize(numSlots_);
      }

      // Compute the kill/gen info for each block.
      if (MF->size() != 1) {
        for (const auto &MBB : *MF) {
          BlockInfo &info = info_[&MBB];
          info.Gen.resize(numSlots_);
          info.Kill.resize(numSlots_);
          auto &gen = info.Gen, &kill = info.Kill;

          for (auto it = MBB.rbegin(); it != MBB.rend(); ++it) {
            for (auto &mem : it->memoperands()) {
              auto *pseudo = mem->getPseudoValue();
              if (auto *stack = llvm::dyn_cast_or_null<StackVal>(pseudo)) {
                auto index = stack->getFrameIndex();
                if (mem->isStore()) {
                  gen[index] = false;
                  kill[index] = true;
                }
                if (mem->isLoad()) {
                  gen[index] = true;
                }
              }
            }
          }

          // Iterative worklist algorithm to compute live variable info.
          llvm::ReversePostOrderTraversal blockOrder(MF);
          bool changed;
          do {
            changed = false;
            for (const auto *MBB : blockOrder) {
              auto &liveInfo = live_[MBB];
              auto &slotOut = liveInfo.SlotOut;
              auto &slotIn = liveInfo.SlotIn;
              for (const auto *SuccMBB : MBB->successors()) {
                for (auto index : live_[SuccMBB].SlotIn.set_bits()) {
                  if (!slotOut[index]) {
                    changed = true;
                    slotOut[index] = true;
                  }
                }
              }

              auto &blockInfo = info_[MBB];
              slotIn = slotOut;
              slotIn.reset(blockInfo.Kill);
              slotIn |= blockInfo.Gen;
            }
          } while (changed);
        }
      }
    } else {
      firstSlot_ = 0;
      numSlots_ = 0;
    }
  }

  /// Computes the set of live locations at an instruction in a block.
  std::vector<unsigned> ComputeLiveAt(MachineInstr *MI)
  {
    auto *MBB = MI->getParent();
    auto *MF = MBB->getParent();

    llvm::BitVector liveSlots = live_[MBB].SlotOut;
    llvm::BitVector liveRegs = live_[MBB].RegOut;
    auto end = ++MI->getReverseIterator();
    for (auto it = MBB->rbegin(); it != end; ++it) {
      // Propagate live spill slots.
      for (auto &mem : it->memoperands()) {
        auto *stack = mem->getPseudoValue();
        if (auto *pseudo = llvm::dyn_cast_or_null<StackVal>(stack)) {
          auto index = pseudo->getFrameIndex();
          if (mem->isStore()) {
            liveSlots[index] = false;
          }
          if (mem->isLoad()) {
            liveSlots[index] = true;
          }
        }
      }

      // Propagate live registers.
      for (auto &op : it->operands()) {
        if (!op.isReg()) {
          continue;
        }
        if (auto regNo = regIndex(op.getReg())) {
          if (op.isDef()) {
            liveRegs[*regNo] = false;
          }
          if (op.isUse() && !it->isCall()) {
            liveRegs[*regNo] = true;
          }
        }
      }
    }

    const auto &TFL = *MF->getSubtarget().getFrameLowering();

    std::vector<unsigned> lives;
    for (auto index : liveSlots.set_bits()) {
      unsigned frameReg;
      auto offset = TFL.getFrameIndexReference(*MF, index, frameReg);
      assert(frameReg == X86::RSP && "invalid frame");
      lives.push_back(offset);
    }
    for (auto reg : liveRegs.set_bits()) {
      lives.push_back((reg << 1) + 1);
    }
    return lives;
  }

private:
  static constexpr unsigned kNumRegs = 15;

  std::optional<unsigned> regIndex(unsigned reg) {
    namespace X86 = llvm::X86;
    switch (reg) {
      case X86::EAX:  case X86::RAX: return 0;
      case X86::EBX:  case X86::RBX: return 1;
      case X86::ESI:  case X86::RSI: return 2;
      case X86::EDI:  case X86::RDI: return 3;
      case X86::EDX:  case X86::RDX: return 4;
      case X86::ECX:  case X86::RCX: return 5;
      case X86::R8D:  case X86::R8:  return 6;
      case X86::R9D:  case X86::R9:  return 7;
      case X86::R12D: case X86::R12: return 8;
      case X86::R13D: case X86::R13: return 9;
      case X86::R10D: case X86::R10: return 10;
      case X86::R11D: case X86::R11: return 11;
      case X86::EBP:  case X86::RBP: return 12;
      case X86::R14D: case X86::R14: return 13;
      case X86::R15D: case X86::R15: return 14;
      default: return std::nullopt;
    }
  }

  unsigned regName(unsigned index) {
    namespace X86 = llvm::X86;
    switch (index) {
      case 0:  return X86::RAX;
      case 1:  return X86::RBX;
      case 2:  return X86::RSI;
      case 3:  return X86::RDI;
      case 4:  return X86::RDX;
      case 5:  return X86::RCX;
      case 6:  return X86::R8;
      case 7:  return X86::R9;
      case 8:  return X86::R12;
      case 9:  return X86::R13;
      case 10: return X86::R10;
      case 11: return X86::R11;
      case 12: return X86::RBP;
      case 13: return X86::R14;
      case 14: return X86::R15;
      default: llvm_unreachable(!"invalid register");
    }
  }

private:
  /// Index of fist spill slot.
  unsigned firstSlot_;
  /// Number of spill slots.
  unsigned numSlots_;

  /// Gen/Kill information for each block.
  struct BlockInfo {
    llvm::BitVector Gen;
    llvm::BitVector Kill;
  };

  /// LiveIn/Live out info.
  struct LiveInfo {
    llvm::BitVector SlotIn;
    llvm::BitVector SlotOut;
    llvm::BitVector RegOut;
  };

  /// Live slot information, per block.
  llvm::DenseMap<const MachineBasicBlock *, LiveInfo> live_;
  /// Kill information for each block.
  llvm::DenseMap<const MachineBasicBlock *, BlockInfo> info_;
};



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
    MachineFunction *MF = (*isel_)[&func];

    // Reset the live variable info - lazily build it if needed.
    lva_ = nullptr;

    for (const auto &block : func) {
      for (const auto &inst : block) {
        if (inst.HasAnnotation(CAML_CALL_FRAME)) {
          LowerCallFrame(MF, &inst);
        }
        if (inst.HasAnnotation(CAML_RAISE_FRAME)) {
          LowerRaiseFrame(MF, &inst);
        }
        if (inst.HasAnnotation(CAML_ROOT_FRAME)) {
          LowerRootFrame(MF, &inst);
        }
      }
    }
  }

  auto *sym = ctx_->getOrCreateSymbol("_caml_genm_frametable");
  auto *ptr = ctx_->createTempSymbol();

  os_->SwitchSection(objInfo_->getDataSection());
  os_->EmitLabel(sym);
  os_->EmitSymbolValue(ptr, 8);
  os_->EmitLabel(ptr);
  os_->EmitIntValue(frames_.size(), 8);
  for (const auto &frame : frames_) {
    LowerFrame(frame);
  }

  return false;
}

// -----------------------------------------------------------------------------
void X86Annot::LowerCallFrame(MachineFunction *MF, const Inst *inst)
{
  auto *func = inst->getParent()->getParent();
  llvm::MCSymbol *symbol = (*isel_)[inst];

  // Find the block containing the call frame.
  MachineInstr *miInst = nullptr;
  for (auto &MBB: *MF) {
    for (auto &MI : MBB) {
      if (!MI.isEHLabel()) {
        continue;
      }

      if (MI.getOperand(0).getMCSymbol() == symbol) {
        miInst = MI.getPrevNode();
        break;
      }
    }
  }
  if (!miInst) {
    MF->dump();
  }
  assert(miInst && "label not found");

  // Compute live variable info.
  if (!lva_) {
    lva_.reset(new X86LVA(func, MF));
  }

  FrameInfo frame;
  frame.Label = symbol;
  frame.FrameSize = MF->getFrameInfo().getStackSize() + 8;
  frame.Live = lva_->ComputeLiveAt(miInst);
  frames_.push_back(frame);
}

// -----------------------------------------------------------------------------
void X86Annot::LowerRaiseFrame(MachineFunction *MF, const Inst *inst)
{
  auto &MFI = MF->getFrameInfo();

  FrameInfo frame;
  frame.Label = (*isel_)[inst];
  frame.FrameSize = MFI.getStackSize() + 8;
  frames_.push_back(frame);
}

// -----------------------------------------------------------------------------
void X86Annot::LowerRootFrame(MachineFunction *MF, const Inst *inst)
{
  auto &MFI = MF->getFrameInfo();

  FrameInfo frame;
  frame.Label = (*isel_)[inst];
  frame.FrameSize = -1;
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
