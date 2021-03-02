// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "passes/global_forward/nodes.h"



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
    if (changed.Contains(id) || Escaped.Contains(id)) {
      Stores.erase(it++);
    } else {
      ++it;
    }
  }
}

// -----------------------------------------------------------------------------
ReverseNodeState::ReverseNodeState(DAGBlock &node) : Node(node) {}

// -----------------------------------------------------------------------------
void ReverseNodeState::Merge(const ReverseNodeState &that)
{
  for (auto it = StorePrecise.begin(); it != StorePrecise.end(); ) {
    if (that.LoadImprecise.Contains(it->first)) {
      StorePrecise.erase(it++);
      continue;
    }

    auto thatLoadIt = that.LoadPrecise.find(it->first);
    auto thatStoreIt = that.StorePrecise.find(it->first);
    for (auto jt = it->second.begin(); jt != it->second.end(); ) {
      auto &[store, end] = jt->second;
      bool killed = false;
      if (!killed && thatLoadIt != that.LoadPrecise.end()) {
        llvm_unreachable("not implemented");
      }
      if (!killed && thatStoreIt != that.StorePrecise.end()) {
        for (auto &[thatStart, thatStoreAndEnd] : thatStoreIt->second) {
          auto &[thatStore, thatEnd] = thatStoreAndEnd;
          if (end <= thatStart || thatEnd <= jt->first) {
            continue;
          }
          if (jt->first == thatStart && end == thatEnd) {
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
      StorePrecise.erase(it++);
    } else {
      ++it;
    }
  }

  for (auto &[id, stores] : that.StorePrecise) {
    if (LoadImprecise.Contains(id)) {
      continue;
    }
    auto thisLoadIt = LoadPrecise.find(id);
    auto thisStoreIt = StorePrecise.find(id);
    for (auto &[start, storeAndEnd] : stores) {
      auto &[store, end] = storeAndEnd;
      bool killed = false;
      if (!killed && thisLoadIt != LoadPrecise.end()) {
        llvm_unreachable("not implemented");
      }
      if (!killed && thisStoreIt != StorePrecise.end()) {
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
        if (thisStoreIt == StorePrecise.end()) {
          StorePrecise[id].emplace(start, storeAndEnd);
        } else {
          thisStoreIt->second.emplace(start, storeAndEnd);
        }
      }
    }
  }

  LoadImprecise.Union(that.LoadImprecise);
  for (const auto &[id, loads] : that.LoadPrecise) {
    LoadPrecise[id].insert(loads.begin(), loads.end());
  }
}

// -----------------------------------------------------------------------------
void ReverseNodeState::Store(ID<Object> id)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void ReverseNodeState::Store(
    ID<Object> id,
    uint64_t start,
    uint64_t end,
    MemoryStoreInst *store)
{
  if (StoreImprecise.Contains(id) || LoadImprecise.Contains(id)) {
    return;
  }
  if (auto it = LoadPrecise.find(id); it != LoadPrecise.end()) {
    for (auto [ldStart, ldEnd] : it->second) {
      if (end <= ldStart || ldEnd <= start) {
        continue;
      }
      if (start == ldStart && end == ldEnd) {
        continue;
      }
      llvm_unreachable("not implemented");
    }
  }
  if (auto it = StorePrecise.find(id); it != StorePrecise.end()) {
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
  StorePrecise[id].emplace(start, std::make_pair(store, end));
}

// -----------------------------------------------------------------------------
void ReverseNodeState::Store(const BitSet<Object> &stored)
{
  StoreImprecise.Union(stored);
}

// -----------------------------------------------------------------------------
void ReverseNodeState::Load(ID<Object> id)
{
  LoadImprecise.Insert(id);
}

// -----------------------------------------------------------------------------
void ReverseNodeState::Load(ID<Object> id, uint64_t start, uint64_t end)
{
  auto storeIt = StorePrecise.find(id);
  if (storeIt == StorePrecise.end()) {
    LoadPrecise[id].emplace(start, end);
  }
}

// -----------------------------------------------------------------------------
void ReverseNodeState::Load(const BitSet<Object> &loaded)
{
  for (auto it = LoadPrecise.begin(); it != LoadPrecise.end(); ) {
    if (loaded.Contains(it->first)) {
      LoadPrecise.erase(it++);
    } else {
      ++it;
    }
  }
  LoadImprecise.Union(loaded);
}

// -----------------------------------------------------------------------------
void ReverseNodeState::Taint(const BitSet<Object> &changed)
{
  StoreImprecise.Union(changed);
  LoadImprecise.Union(changed);

  for (auto it = LoadPrecise.begin(); it != LoadPrecise.end(); ) {
    if (changed.Contains(it->first)) {
      LoadPrecise.erase(it++);
    } else {
      ++it;
    }
  }
}

// -----------------------------------------------------------------------------
void ReverseNodeState::dump(llvm::raw_ostream &os)
{
  os << "\tLoad: " << LoadImprecise << "\n";
  for (auto &[id, loads] : LoadPrecise) {
    for (auto &[start, end] : loads) {
      os << "\t\t" << id << " + " << start << "," << end << "\t";
    }
  }
  os << "\tStore: " << StoreImprecise << "\n";
  for (auto &[id, stores] : StorePrecise) {
    for (auto &[off, storeAndEnd] : stores) {
      auto &[store, end] = storeAndEnd;
      os << "\t\t" << id << " + " << off << "," << end << "\n";
    }
  }
}
