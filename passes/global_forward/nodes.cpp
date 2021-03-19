// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/Support/Debug.h>

#include "passes/global_forward/nodes.h"
#include "core/type.h"
#include "core/inst.h"

#define DEBUG_TYPE "global-forward"



// -----------------------------------------------------------------------------
void NodeState::Merge(const NodeState &that)
{
  Funcs.Union(that.Funcs);
  Escaped.Union(that.Escaped);

  Stored.Union(that.Stored);

  auto thisIt = Stores.begin();
  auto thatIt = that.Stores.begin();
  while (thisIt != Stores.end() && thatIt != that.Stores.end()) {
    while (thisIt != Stores.end() && thisIt->first < thatIt->first) {
      Stores.erase(thisIt++);
    }
    if (thisIt == Stores.end()) {
      break;
    }
    while (thatIt != that.Stores.end() && thatIt->first < thisIt->first) {
      ++thatIt;
    }
    if (thatIt == that.Stores.end()) {
      break;
    }
    if (thisIt->first == thatIt->first) {
      if (thisIt->second != thatIt->second) {
        auto &thisMap = thisIt->second;
        auto &thatMap = thatIt->second;
        for (auto it = thisMap.begin(); it != thisMap.end(); ) {
          auto tt = thatMap.find(it->first);
          if (tt == thatMap.end()) {
            thisMap.erase(it++);
            continue;
          }
          if (*it != *tt) {
            thisMap.erase(it++);
          } else {
            ++it;
          }
        }
        if (thisMap.empty()) {
          Stores.erase(thisIt++);
        } else {
          ++thisIt;
        }
      } else {
        ++thisIt;
      }
      ++thatIt;
    }
  }
  Stores.erase(thisIt, Stores.end());
}

// -----------------------------------------------------------------------------
void NodeState::Overwrite(const BitSet<Object> &changed)
{
  Stored.Union(changed);
  for (auto it = Stores.begin(); it != Stores.end(); ) {
    auto id = it->first;
    if (changed.Contains(id)) {
      Stores.erase(it++);
    } else {
      ++it;
    }
  }
}

// -----------------------------------------------------------------------------
void NodeState::dump(llvm::raw_ostream &os)
{
  os << "\tEscaped: " << Escaped << "\n";
  os << "\tStored: " << Stored << "\n";
  for (auto &[id, stores] : Stores) {
    for (auto &[off, storeAndTy] : stores) {
      auto &[ty, inst] = storeAndTy;
      os << "\t\t" << id << " + " << off << "," << off + GetSize(ty);
      if (inst) {
        os << *inst;
      }
      os << "\n";
    }
  }
}

// -----------------------------------------------------------------------------
ReverseNodeState::ReverseNodeState(DAGBlock &node) : Node(node) {}

// -----------------------------------------------------------------------------
void ReverseNodeState::Merge(const ReverseNodeState &that)
{
  for (auto it = Stores.begin(); it != Stores.end(); ) {
    if (that.Loaded.Contains(it->first)) {
      Stores.erase(it++);
      continue;
    }

    auto thatStoreIt = that.Stores.find(it->first);
    for (auto jt = it->second.begin(); jt != it->second.end(); ) {
      auto start = jt->first;
      auto &[store, end] = jt->second;
      bool killed = false;
      if (thatStoreIt != that.Stores.end()) {
        for (auto &[thatStart, thatStoreAndEnd] : thatStoreIt->second) {
          auto &[thatStore, thatEnd] = thatStoreAndEnd;
          if (end <= thatStart || thatEnd <= start) {
            continue;
          }
          if (start == thatStart && end == thatEnd) {
            continue;
          }
          llvm_unreachable("not implemented");
        }
      }
      if (killed) {
        it->second.erase(jt++);
      } else {
        ++jt;
      }
    }

    if (it->second.empty()) {
      Stores.erase(it++);
    } else {
      ++it;
    }
  }

  for (auto &[id, stores] : that.Stores) {
    if (Loaded.Contains(id)) {
      continue;
    }
    auto thisStoreIt = Stores.find(id);
    for (auto &[start, storeAndEnd] : stores) {
      auto &[store, end] = storeAndEnd;
      bool killed = false;
      if (thisStoreIt != Stores.end()) {
        for (auto &[thisStart, thisStoreAndEnd] : thisStoreIt->second) {
          auto &[thisStore, thisEnd] = thisStoreAndEnd;
          if (end <= thisStart || thisEnd <= start) {
            continue;
          }
          if (start == thisStart && end == thisEnd) {
            continue;
          }
          llvm_unreachable("not implemented");
        }
      }
      if (!killed) {
        if (thisStoreIt == Stores.end()) {
          Stores[id].emplace(start, storeAndEnd);
        } else {
          thisStoreIt->second.emplace(start, storeAndEnd);
        }
      }
    }
  }

  Loaded.Union(that.Loaded);
}

// -----------------------------------------------------------------------------
void ReverseNodeState::Store(ID<Object> id)
{
  Stored.Insert(id);
}

// -----------------------------------------------------------------------------
void ReverseNodeState::Store(
    ID<Object> id,
    uint64_t start,
    uint64_t end,
    MemoryStoreInst *store)
{
  if (Stored.Contains(id) || Loaded.Contains(id)) {
    return;
  }
  if (auto it = Stores.find(id); it != Stores.end()) {
    for (auto &[stStart, instAndEnd] : it->second) {
      auto &[inst, stEnd] = instAndEnd;
      if (end <= stStart || stEnd <= start) {
        continue;
      } else if (start == stStart && end == stEnd) {
        return;
      } else {
        llvm_unreachable("not implemented");
      }
    }
  }
  LLVM_DEBUG({
    llvm::dbgs() << "\t\t\trecord: " << id << " + " << start << "," << end;
    if (store) {
      llvm::dbgs() << *store;
    }
    llvm::dbgs() << "\n";
  });
  Stores[id].emplace(start, std::make_pair(store, end));
}

// -----------------------------------------------------------------------------
void ReverseNodeState::Store(const BitSet<Object> &changed)
{
  Stored.Union(changed);
}

// -----------------------------------------------------------------------------
void ReverseNodeState::Load(ID<Object> id)
{
  Loaded.Insert(id);
}

// -----------------------------------------------------------------------------
void ReverseNodeState::Load(ID<Object> id, uint64_t start, uint64_t end)
{
  bool killed = false;
  if (auto storeIt = Stores.find(id); storeIt != Stores.end()) {
    for (auto &[stStart, storeAndEnd] : storeIt->second) {
      auto stEnd = storeAndEnd.second;
      if (end <= stStart ||  stEnd <= start) {
        continue;
      }
      killed = true;
      break;
    }
  }
  if (!killed) {
    Loaded.Insert(id);
  }
}

// -----------------------------------------------------------------------------
void ReverseNodeState::Load(const BitSet<Object> &loaded)
{
  Loaded.Union(loaded);
}

// -----------------------------------------------------------------------------
void ReverseNodeState::dump(llvm::raw_ostream &os)
{
  os << "\tLoad: " << Loaded << "\n";
  os << "\tStore: " << Stored << "\n";
  for (auto &[id, stores] : Stores) {
    for (auto &[off, storeAndEnd] : stores) {
      auto &[inst, end] = storeAndEnd;
      os << "\t\t" << id << " + " << off << "," << end;
      if (inst) {
        os << *inst;
      }
      os << "\n";
    }
  }
}
