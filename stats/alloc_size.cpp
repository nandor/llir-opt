// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/block.h"
#include "core/cast.h"
#include "core/func.h"
#include "core/insts.h"
#include "core/prog.h"
#include "stats/alloc_size.h"



// -----------------------------------------------------------------------------
static std::optional<int64_t> GetConstant(Inst *inst)
{
  if (auto *movInst = ::dyn_cast_or_null<MovInst>(inst)) {
    if (auto *value = ::dyn_cast_or_null<ConstantInt>(movInst->GetArg())) {
      if (value->GetValue().getMinSignedBits() >= 64) {
        return value->GetInt();
      }
    }
  }
  return {};
}

// -----------------------------------------------------------------------------
const char *AllocSizePass::kPassID = "alloc-size";

// -----------------------------------------------------------------------------
void AllocSizePass::Run(Prog *prog)
{
  for (Func &func : *prog) {
    for (Block &block : func) {
      for (Inst &inst : block) {
        switch (inst.GetKind()) {
          case Inst::Kind::CALL:
              AnalyseCall(static_cast<CallSite<ControlInst> &>(inst));
              continue;
          case Inst::Kind::INVOKE:
          case Inst::Kind::TCALL:
          case Inst::Kind::TINVOKE:
            AnalyseCall(static_cast<CallSite<TerminatorInst> &>(inst));
            continue;
          default:
            continue;
        }
      }
    }
  }

  llvm::outs() << "Allocation Size Statistics:\n";
  llvm::outs() << "\tKnown:     " << numKnownAllocs << "\n";
  llvm::outs() << "\tTruncated: " << numTruncatedAllocs << "\n";
  llvm::outs() << "\tUnknown:   " << numUnknownAllocs << "\n";
}

// -----------------------------------------------------------------------------
template <typename T>
void AllocSizePass::AnalyseCall(CallSite<T> &inst)
{
  if (auto *movInst = ::dyn_cast_or_null<MovInst>(inst.GetCallee())) {
    if (auto *callee = ::dyn_cast_or_null<Global>(movInst->GetArg())) {
      // If the target is a known callee, figure out the size or substitute it
      // with a sensible default value, which is 128 bytes. All values stored
      // outside the fixed range are stored in a separate out-of-bounds set.
      const auto &name = callee->getName();
      if (name.substr(0, 10) == "caml_alloc") {
        const auto &k = name.substr(10);
        if (k == "1") {
          AnalyseAlloc(16);
          return;
        }
        if (k == "2") {
          AnalyseAlloc(24);
          return;
        }
        if (k == "3") {
          AnalyseAlloc(32);
          return;
        }
        if (k == "N") {
          AnalyseAlloc(GetConstant(*inst.arg_begin()));
          return;
        }
        if (k == "_young" || k == "_small") {
          if (auto n = GetConstant(*inst.arg_begin())) {
            AnalyseAlloc(*n * 8 + 8);
          } else {
            AnalyseAlloc(std::nullopt);
          }
          return;
        }
      }
      if (name == "malloc") {
        AnalyseAlloc(GetConstant(*inst.arg_begin()));
        return;
      }
    }
  }
}

// -----------------------------------------------------------------------------
void AllocSizePass::AnalyseAlloc(const std::optional<int64_t> &size)
{
  if (size) {
    if (*size > 16 * 8) {
      numTruncatedAllocs += 1;
    } else {
      numKnownAllocs += 1;
    }
  } else {
    numUnknownAllocs += 1;
  }
}

// -----------------------------------------------------------------------------
const char *AllocSizePass::GetPassName() const
{
  return "Allocation Size Statistics";
}
