// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/PostOrderIterator.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/cfg.h"
#include "core/func.h"
#include "core/prog.h"
#include "core/insts.h"
#include "core/target.h"
#include "passes/merge_stores.h"

#define DEBUG_TYPE "merge-stores"



// -----------------------------------------------------------------------------
class StoreSequenceMerger {
public:
  StoreSequenceMerger(Block &block, const Target *target) 
    : block_(block)
    , target_(target)
  {
  }

  void Run();

private:
  StoreInst *MergeTruncStores(llvm::ArrayRef<StoreInst *> stores);

private:
  Block &block_;
  const Target *target_;
};

// -----------------------------------------------------------------------------
void StoreSequenceMerger::Run()
{
  std::vector<StoreInst *> stores;
  for (auto it = block_.begin(); it != block_.end(); ) {
    Inst &inst = *it++;
    if (auto *store = ::cast_or_null<StoreInst>(&inst)) {
      if (auto frame = ::cast_or_null<FrameInst>(store->GetAddr())) {
        for (auto st = stores.begin(); st != stores.end(); ) {
          auto addr = ::cast<FrameInst>((*st)->GetAddr());
          if (frame->GetObject() == addr->GetObject()) {
            auto st0 = frame->GetOffset();
            auto en0 = st0 + GetSize(store->GetValue().GetType());
            auto st1 = addr->GetOffset();
            auto en1 = st1 + GetSize((*st)->GetValue().GetType());
            if (!(en1 <= st0 || en0 <= st1)) {
              st = stores.erase(st);
              continue;
            }
          }
          ++st;
        }
        stores.push_back(store);
        if (auto *merged = MergeTruncStores(stores)) {
          stores.clear();
          block_.insert(merged, it);
        }
      } else {
        stores.clear();
      }
      continue;
    }
    if (inst.HasSideEffects()) {
      stores.clear();
    }
  }
}

// -----------------------------------------------------------------------------
StoreInst *StoreSequenceMerger::MergeTruncStores(
    llvm::ArrayRef<StoreInst *> stores)
{
  std::unordered_map
      < Ref<Inst>
      , std::map<unsigned, std::pair<unsigned, StoreInst *>>
      > byteAtOffset;
  for (auto *store : stores) {
    auto base = ::cast<FrameInst>(store->GetAddr())->GetOffset();
    if (auto trunc = ::cast_or_null<TruncInst>(store->GetValue())) {
      auto arg = trunc->GetArg();
      if (auto shift = ::cast_or_null<SrlInst>(arg)) {
        if (auto mov = ::cast_or_null<MovInst>(shift->GetRHS())) {
          if (auto off = ::cast_or_null<ConstantInt>(mov->GetArg())) {
            if (off->GetInt() % 8 == 0) {
              auto start = off->GetInt() / 8;
              for (unsigned i = 0; i < GetSize(arg.GetType()); ++i) {
                byteAtOffset[shift->GetLHS()].emplace(
                    start,
                    std::make_pair(base, store)
                );
              }
            }
          }
        }
      } else {
        byteAtOffset[arg].emplace(0, std::make_pair(base, store));
      }
    }
  }

  if (target_->IsLittleEndian() && target_->AllowsUnalignedStores()) {
    for (auto &[ref, places] : byteAtOffset) {
      unsigned startFrom = 0;
      unsigned startTo = places.begin()->second.first;
      bool valid = true;
      Ref<Inst> offStart;
      llvm::SmallVector<StoreInst *, 8> stores;
      for (auto &[from, toAndStore] : places) {
        auto [to, store] = toAndStore;
        if (from != startFrom || to != startTo) {
          valid = false;
          break;
        }
        auto size = store->GetValue();
        startFrom += size;
        startTo += size;
        if (!offStart) {
          offStart = store->GetAddr();
        }
        stores.push_back(store);
      }
      auto refType = ref.GetType();
      auto refSize = GetSize(refType);
      if (valid && startFrom == refSize) {
        for (auto *store : stores) {
          store->eraseFromParent();
        }
        return new StoreInst(offStart, ref, {});
      }
    }
  } 
  return nullptr;
}

// -----------------------------------------------------------------------------
const char *MergeStoresPass::kPassID = DEBUG_TYPE;

// -----------------------------------------------------------------------------
bool MergeStoresPass::Run(Prog &prog)
{
  bool changed = false;
  for (Func &func : prog) {
    for (Block &block : func) {
      StoreSequenceMerger(block, GetTarget()).Run();
    }
  }
  return changed;
}

// -----------------------------------------------------------------------------
const char *MergeStoresPass::GetPassName() const
{
  return "Store Merging";
}
