// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/BitVector.h>
#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/CodeGen/LiveVariables.h>
#include <llvm/CodeGen/MachineInstrBuilder.h>
#include <llvm/CodeGen/MachineModuleInfo.h>
#include <llvm/IR/Mangler.h>
#include <llvm/MC/MCObjectFileInfo.h>
#include <llvm/MC/MCStreamer.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/cfg.h"
#include "core/func.h"
#include "core/prog.h"
#include "emitter/x86/x86annot.h"
#include "emitter/x86/x86isel.h"

namespace X86 = llvm::X86;
namespace TargetOpcode = llvm::TargetOpcode;
using TargetInstrInfo = llvm::TargetInstrInfo;
using StackVal = llvm::FixedStackPseudoSourceValue;



// -----------------------------------------------------------------------------
char X86Annot::ID;


// -----------------------------------------------------------------------------
static std::optional<unsigned> RegIndex(unsigned reg)
{
  switch (reg) {
    case X86::RAX: return 0;
    case X86::RBX: return 1;
    case X86::RDI: return 2;
    case X86::RSI: return 3;
    case X86::RDX: return 4;
    case X86::RCX: return 5;
    case X86::R8:  return 6;
    case X86::R9:  return 7;
    case X86::R12: return 8;
    case X86::R13: return 9;
    case X86::R10: return 10;
    case X86::R11: return 11;
    case X86::RBP: return 12;
    case X86::R14: return 13;
    case X86::R15: return 14;
    default: return std::nullopt;
  }
}

// -----------------------------------------------------------------------------
X86Annot::X86Annot(
    llvm::MCContext *ctx,
    llvm::MCStreamer *os,
    const llvm::MCObjectFileInfo *objInfo,
    const llvm::DataLayout &layout)
  : llvm::ModulePass(ID)
  , ctx_(ctx)
  , os_(os)
  , objInfo_(objInfo)
  , layout_(layout)
{
}

// -----------------------------------------------------------------------------
bool X86Annot::runOnModule(llvm::Module &M)
{
  auto &MMI = getAnalysis<llvm::MachineModuleInfoWrapperPass>().getMMI();

  for (auto &F : M) {
    auto &MF = MMI.getOrCreateMachineFunction(F);
    const auto *TFL = MF.getSubtarget().getFrameLowering();
    const auto *TII = MF.getSubtarget().getInstrInfo();
    for (auto &MBB : MF) {
      // Find all roots and emit frames for them.
      // The label is emitted by AsmPrinter later.
      for (auto it = MBB.instr_begin(); it != MBB.instr_end(); ) {
        auto &MI = *it++;
        switch (MI.getOpcode()) {
          case TargetOpcode::GC_FRAME_ROOT: {
            roots_.push_back(MI.getOperand(0).getMCSymbol());
            break;
          }
          case TargetOpcode::GC_FRAME_CALL: {
            FrameInfo frame;
            frame.Label = MI.getOperand(0).getMCSymbol();
            frame.FrameSize = MF.getFrameInfo().getStackSize() + 8;
            for (unsigned i = 1, n = MI.getNumOperands(); i < n; ++i) {
              auto &op = MI.getOperand(i);
              if (op.isReg()) {
                if (auto regNo = op.getReg(); regNo > 0) {
                  if (auto reg = RegIndex(regNo)) {
                    // Actual register - yay.
                    frame.Live.insert((*reg << 1) | 1);
                  } else {
                    // Regalloc should ensure this is valid.
                    llvm_unreachable("invalid live reg");
                  }
                }
                continue;
              }
              if (op.isImm()) {
                frame.Allocs.push_back(op.getImm());
                continue;
              }
              if (op.isRegMask()) {
                // Ignore the reg mask.
                continue;
              }
              llvm_unreachable("invalid operand kind");
            }
            for (auto *mop : MI.memoperands()) {
              auto *pseudo = mop->getPseudoValue();
              if (auto *stack = llvm::dyn_cast_or_null<StackVal>(pseudo)) {
                auto index = stack->getFrameIndex();
                llvm::Register frameReg;
                auto offset = TFL->getFrameIndexReference(MF, index, frameReg);
                assert(frameReg == X86::RSP && "invalid frame");
                frame.Live.insert(offset);
                continue;
              }
              llvm_unreachable("invalid live spill");
            }

            frames_.push_back(frame);
            break;
          }
          default: {
            // Nothing to emit for others.
            continue;
          }
        }
      }
    }
  }

  if (!frames_.empty() || !roots_.empty()) {
    os_->SwitchSection(objInfo_->getDataSection());
    os_->emitValueToAlignment(8);
    os_->emitLabel(LowerSymbol("caml_llir_frametable"));
    os_->emitIntValue(frames_.size() + roots_.size(), 8);
    for (const auto &frame : frames_) {
      LowerFrame(frame);
    }
    for (const auto *root : roots_) {
      os_->emitSymbolValue(root, 8);
      os_->emitIntValue(0xFFFF, 2);
      os_->emitIntValue(0, 2);
      os_->emitIntValue(0, 1);
      os_->emitValueToAlignment(4);
      os_->emitIntValue(0, 8);
      os_->emitValueToAlignment(8);
    }
  }

  return false;
}

// -----------------------------------------------------------------------------
void X86Annot::LowerFrame(const FrameInfo &info)
{
  uint16_t flags = info.FrameSize;
  if (!info.Allocs.empty()) {
    flags |= 2;
  }

  os_->emitSymbolValue(info.Label, 8);
  os_->emitIntValue(flags, 2);
  os_->emitIntValue(info.Live.size(), 2);
  for (auto live : info.Live) {
    if ((live & 1) == 1) {
      os_->emitIntValue(live, 2);
    }
  }
  for (auto live : info.Live) {
    if ((live & 1) == 0) {
      os_->emitIntValue(live, 2);
    }
  }

  if (!info.Allocs.empty()) {
    os_->emitIntValue(info.Allocs.size(), 1);
    for (auto alloc : info.Allocs) {
      assert(2 <= alloc && alloc - 2 <= std::numeric_limits<uint8_t>::max());
      os_->emitIntValue(alloc - 2, 1);
    }
  }

  os_->emitValueToAlignment(8);
}

// -----------------------------------------------------------------------------
llvm::MCSymbol *X86Annot::LowerSymbol(const std::string_view name)
{
  llvm::SmallString<128> sym;
  llvm::Mangler::getNameWithPrefix(sym, name.data(), layout_);
  return ctx_->getOrCreateSymbol(sym);
}

// -----------------------------------------------------------------------------
llvm::StringRef X86Annot::getPassName() const
{
  return "LLIR X86 Annotation Inserter";
}

// -----------------------------------------------------------------------------
void X86Annot::getAnalysisUsage(llvm::AnalysisUsage &AU) const
{
  AU.addRequired<llvm::MachineModuleInfoWrapperPass>();
  AU.addPreserved<llvm::MachineModuleInfoWrapperPass>();
}
