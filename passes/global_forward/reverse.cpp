// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/Statistic.h>
#include <llvm/Support/Debug.h>

#include "passes/global_forward/forwarder.h"

#define DEBUG_TYPE "global-forward"

STATISTIC(NumStoresFolded, "Stores folded");



// -----------------------------------------------------------------------------
bool GlobalForwarder::Reverse()
{
  std::vector<ReverseNodeState *> nodes;
  std::unordered_set<ReverseNodeState *> visited;
  std::function<void(ReverseNodeState *node)> dfs =
    [&, this] (ReverseNodeState *node)
    {
      if (!visited.insert(node).second) {
        return;
      }
      // Compute information for all the successors in the DAG.
      for (auto *succ : node->Succs) {
        dfs(succ);
      }
      nodes.push_back(node);
    };

  auto *entry = &GetReverseNode(entry_, GetDAG(entry_).rbegin()->Index);
  dfs(entry);

  LLVM_DEBUG(llvm::dbgs() << "===================\n");
  LLVM_DEBUG(llvm::dbgs() << "Reverse:\n");
  LLVM_DEBUG(llvm::dbgs() << "===================\n");

  bool changed = false;
  for (ReverseNodeState *node : nodes) {
    LLVM_DEBUG(llvm::dbgs() << "===================\n");
    LLVM_DEBUG(llvm::dbgs() << node->Node << "\n");
    LLVM_DEBUG(llvm::dbgs() << "===================\n");
    // Merge information from successors.
    std::optional<ReverseNodeState> merged;
    LLVM_DEBUG(llvm::dbgs() << "Merged:\n");
    for (auto *succ : node->Succs) {
      LLVM_DEBUG(llvm::dbgs() << "\t" << succ->Node << "\n");
      if (merged) {
        merged->Merge(*succ);
      } else {
        merged.emplace(*succ);
      }
    }
    #ifndef DEBUG
    if (merged) {
      LLVM_DEBUG(merged->dump(llvm::dbgs()));
      LLVM_DEBUG(node->dump(llvm::dbgs()));
    }
    #endif
    // Apply the transfer function.
    if (merged) {
      auto stores = node->Stores;
      for (auto &&[id, stores] : merged->Stores) {
        if (node->Stored.Contains(id)) {
          continue;
        }
        auto storeIt = node->Stores.find(id);
        for (auto &[start, instAndEnd] : stores) {
          auto &[inst, end] = instAndEnd;

          bool killed = false;
          if (storeIt != node->Stores.end()) {
            for (auto &[nodeStart, nodeInstAndEnd] : storeIt->second) {
              auto &[nodeInst, nodeEnd] = nodeInstAndEnd;
              if (end <= nodeStart || nodeEnd <= start) {
                continue;
              }
              if (start == nodeStart && end == nodeEnd) {
                killed = true;
                break;
              }
              llvm_unreachable("not implemented");
            }
          }
          if (!killed) {
            node->Stores[id].emplace(start, instAndEnd);
          }
        }
      }

      for (auto &&[id, stores] : node->Stores) {
        auto *obj = idToObject_[id];
        if (obj->size() != 1) {
          continue;
        }
        Atom *atom = &*obj->begin();
        for (auto &[start, storeAndEnd] : stores) {
          auto [store, end] = storeAndEnd;
          if (start != 0 || end != atom->GetByteSize()) {
            continue;
          }
          merged->Loaded.Erase(id);
          break;
        }
      }

      node->Loaded.Union(merged->Loaded);
    }
    LLVM_DEBUG(llvm::dbgs() << "Final:\n");
    #ifndef DEBUG
    LLVM_DEBUG(node->dump(llvm::dbgs()));
    #endif
  };

  for (auto &[id, stores] : entry->Stores) {
    auto *object = idToObject_[id];
    if (!object) {
      continue;
    }

    for (auto &[off, instAndEnd] : stores) {
      auto &[store, end] = instAndEnd;
      if (!store) {
        continue;
      }
      auto mov = ::cast_or_null<MovInst>(store->GetValue());
      if (!mov || !mov->GetArg()->IsConstant()) {
        continue;
      }
      if (object->Store(off, mov->GetArg(), mov.GetType())) {
        LLVM_DEBUG(llvm::dbgs() << "Folded store: " << *store->GetAddr() << "\n");
        store->eraseFromParent();
        NumStoresFolded++;
        changed = true;
      }
    }
  }
  return changed;
}
