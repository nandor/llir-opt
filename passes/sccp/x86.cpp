// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/target/x86.h"
#include "passes/sccp/lattice.h"
#include "passes/sccp/solver.h"
#include "passes/sccp/eval.h"



// -----------------------------------------------------------------------------
static std::pair<uint32_t, uint32_t>
GetFeatureFlags(const llvm::X86Subtarget &sti)
{
  uint32_t rcx = 0;
  // OSXSAVE flag
  rcx |= sti.hasAVX() ? (1 << 27) : 0;

  uint32_t rdx = 0;

  return std::make_pair(rcx, rdx);
}

// -----------------------------------------------------------------------------
void SCCPSolver::VisitX86_CpuIdInst(X86_CpuIdInst &inst)
{
  // Get the leaf value, as an integer.
  auto ll = GetValue(inst.GetLeaf());
  if (ll.IsUnknown()) {
    return;
  }
  auto vl = ll.AsInt();
  if (!vl || vl->getBitWidth() >= 64) {
    MarkOverdefined(inst);
    return;
  }

  // Helper to get the subleaf.
  auto subleaf = [&, this] () -> std::optional<int32_t>
  {
    if (auto node = inst.GetSubleaf()) {
      auto ls = GetValue(inst.GetSubleaf());
      if (ls.IsUnknown()) {
        return std::nullopt;
      }
      auto vs = ls.AsInt();
      if (!vs || vs->getBitWidth() >= 64) {
        MarkOverdefined(inst);
        return std::nullopt;
      }
      return vs->getZExtValue();
    }
    return std::nullopt;
  };

  // Fetch the X86 subtarget.
  auto subtarget = [&, this] () -> const llvm::X86Subtarget *
  {
    if (!target_) {
      return nullptr;
    }
    if (auto *x86target = target_->As<X86Target>()) {
      const auto &func = *inst.getParent()->getParent();
      return &x86target->GetSubtarget(func);
    }
    return nullptr;
  };

  switch (vl->getZExtValue()) {
    default: {
      MarkOverdefined(inst);
      return;
    }
    case 0x0: {
      MarkOverdefined(inst);
      return;
    }
    case 0x1: {
      if (auto *sti = subtarget()) {
        // Find the feature flags.
        auto [rcx, rdx] = GetFeatureFlags(*sti);

        // AX: model information.
        Mark(inst.GetSubValue(0), Lattice::Overdefined());
        // BX: processor info.
        Mark(inst.GetSubValue(1), Lattice::Overdefined());

        // CX: feature flags.
        auto rcxRef = inst.GetSubValue(2);
        llvm::APInt rcxV(32, rcx, true);
        Mark(rcxRef, SCCPEval::Extend(
            rcx ? Lattice::CreateMask(rcxV, rcxV) : Lattice::Overdefined(),
            rcxRef.GetType()
        ));

        // DX: feature flags.
        auto rdxRef = inst.GetSubValue(3);
        llvm::APInt rdxV(32, rdx, true);
        Mark(rdxRef, SCCPEval::Extend(
            rdx ? Lattice::CreateMask(rdxV, rdxV) : Lattice::Overdefined(),
            rcxRef.GetType()
        ));
      } else {
        MarkOverdefined(inst);
      }
      return;
    }
    case 0xD: {
      if (auto ecx = subleaf()) {
        switch (*ecx) {
          case 0: {
            MarkOverdefined(inst);
            break;
          }
          case 1: {
            if (auto *sti = subtarget()) {
              llvm::APInt m(32, sti->hasXSAVEOPT(), true);
              Mark(
                  inst.GetSubValue(0),
                  m.isNullValue() ? Lattice::Overdefined() : Lattice::CreateMask(m, m)
              );
              Mark(inst.GetSubValue(1), Lattice::Overdefined());
              Mark(inst.GetSubValue(2), Lattice::Overdefined());
              Mark(inst.GetSubValue(3), Lattice::Overdefined());
            } else {
              MarkOverdefined(inst);
            }
            break;
          }
          default: {
            MarkOverdefined(inst);
            break;
          }
        }
      }
      return;
    }
  }
}
