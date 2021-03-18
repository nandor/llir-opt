// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "passes/global_forward/nodes.h"
#include "core/inst.h"



// -----------------------------------------------------------------------------
void NodeState::Merge(const NodeState &that)
{
  Funcs.Union(that.Funcs);
  Escaped.Union(that.Escaped);

  StoredImprecise.Union(that.StoredImprecise);
  for (auto &[object, offsets] : that.StoredPrecise) {
    if (!StoredImprecise.Contains(object)) {
      for (auto &off : offsets) {
        StoredPrecise[object].insert(off);
      }
    }
  }
  for (auto it = StoredPrecise.begin(); it != StoredPrecise.end(); ) {
    if (StoredImprecise.Contains(it->first)) {
      StoredPrecise.erase(it++);
    } else {
      ++it;
    }
  }

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
void NodeState::Overwrite(
    const BitSet<Object> &imprecise,
    const ObjectOffsetMap &precise)
{
  StoredImprecise.Union(imprecise);
  for (auto &[object, offsets] : precise) {
    if (!StoredImprecise.Contains(object)) {
      for (auto &off : offsets) {
        StoredPrecise[object].insert(off);
      }
    }
  }
  for (auto it = StoredPrecise.begin(); it != StoredPrecise.end(); ) {
    if (StoredImprecise.Contains(it->first)) {
      StoredPrecise.erase(it++);
    } else {
      ++it;
    }
  }

  for (auto it = Stores.begin(); it != Stores.end(); ) {
    auto id = it->first;
    if (imprecise.Contains(id)) {
      Stores.erase(it++);
    } else {
      for (auto et = it->second.begin(); et != it->second.end(); ) {
        auto stStart = et->first;
        const auto stEnd = stStart + GetSize(et->second.first);

        bool killed = false;
        if (auto pt = precise.find(id); pt != precise.end()) {
          for (auto &[start, end] : pt->second) {
            if (end <= stStart || stEnd <= start) {
              continue;
            }
            killed = true;
            break;
          }
        }

        if (killed) {
          it->second.erase(et++);
        } else {
          ++et;
        }
      }
      if (it->second.empty()) {
        Stores.erase(it++);
      } else {
        ++it;
      }
    }
  }
}

// -----------------------------------------------------------------------------
void NodeState::dump(llvm::raw_ostream &os)
{
  os << "\tEscaped: " << Escaped << "\n";
  os << "\tStored: " << StoredImprecise << "\n";
  for (auto &[id, offsets] : StoredPrecise) {
    for (auto [start, end] : offsets) {
      os << "\t\t" << id << " + " << start << "," << end << "\n";
    }
  }
  for (auto &[id, stores] : Stores) {
    for (auto &[off, storeAndEnd] : stores) {
      auto &[ty, inst] = storeAndEnd;
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
  for (auto it = StorePrecise.begin(); it != StorePrecise.end(); ) {
    if (that.LoadImprecise.Contains(it->first)) {
      StorePrecise.erase(it++);
      continue;
    }

    auto thatLoadIt = that.LoadPrecise.find(it->first);
    auto thatStoreIt = that.StorePrecise.find(it->first);
    for (auto jt = it->second.begin(); jt != it->second.end(); ) {
      auto start = jt->first;
      auto &[store, end] = jt->second;
      bool killed = false;
      if (!killed && thatLoadIt != that.LoadPrecise.end()) {
        for (auto [thatStart, thatEnd] : thatLoadIt->second) {
          if (end <= thatStart || thatEnd <= start) {
            continue;
          }
          killed = true;
          break;
        }
      }
      if (!killed && thatStoreIt != that.StorePrecise.end()) {
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
        for (auto [thisStart, thisEnd] : thisLoadIt->second) {
          if (end <= thisStart || thisEnd <= start) {
            continue;
          }
          killed = true;
          break;
        }
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
  StoreImprecise.Insert(id);
  KillableStores.erase(id);
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
        return;
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
  if (auto it = KillableStores.find(id); it != KillableStores.end()) {
    for (auto et = it->second.begin(); et != it->second.end(); ) {
      auto stStart = et->first;
      auto stEnd = et->second.second;
      if (stEnd <= start || end <= stStart) {
        ++et;
        continue;
      }
      llvm_unreachable("not implemented");
    }
  }
  StorePrecise[id].emplace(start, std::make_pair(store, end));
  KillableStores[id].emplace(start, std::make_pair(store, end));
}

// -----------------------------------------------------------------------------
void ReverseNodeState::Store(
    const BitSet<Object> &imprecise,
    const ObjectOffsetMap &precise)
{
  StoreImprecise.Union(imprecise);
  for (auto it = StorePrecise.begin(); it != StorePrecise.end(); ) {
    if (StoreImprecise.Contains(it->first)) {
      StorePrecise.erase(it++);
    } else {
      ++it;
    }
  }
  for (auto &[object, offsets] : precise) {
    for (auto &off : offsets) {
      StorePrecise[object].emplace(
          off.first,
          std::make_pair(nullptr, off.second)
      );
    }
  }

  for (auto it = KillableStores.begin(); it != KillableStores.end(); ) {
    if (imprecise.Contains(it->first)) {
      KillableStores.erase(it++);
    } else {
      if (auto pt = precise.find(it->first); pt != precise.end()) {
        for (auto et = it->second.begin(); et != it->second.end(); ) {
          llvm_unreachable("not implemented");
        }
      }
      if (it->second.empty()) {
        KillableStores.erase(it++);
      } else {
        ++it;
      }
    }
  }
}

// -----------------------------------------------------------------------------
void ReverseNodeState::Load(ID<Object> id)
{
  LoadPrecise.erase(id);
  LoadImprecise.Insert(id);
  KillableStores.erase(id);
}

// -----------------------------------------------------------------------------
void ReverseNodeState::Load(ID<Object> id, uint64_t start, uint64_t end)
{
  if (auto storeIt = StorePrecise.find(id); storeIt == StorePrecise.end()) {
    LoadPrecise[id].emplace(start, end);
  }
  if (auto killIt = KillableStores.find(id); killIt != KillableStores.end()) {
    for (auto et = killIt->second.begin(); et != killIt->second.end(); ) {
      auto stStart = et->first;
      auto stEnd = et->second.second;
      if (end <= stStart || stEnd <= start) {
        ++et;
      } else {
        killIt->second.erase(et++);
      }
    }
  }
}

// -----------------------------------------------------------------------------
void ReverseNodeState::Load(
    const BitSet<Object> &imprecise,
    const ObjectOffsetMap &precise)
{
  for (auto &[object, offsets] : precise) {
    for (auto &off : offsets) {
      bool shadowed = false;
      if (auto pt = StorePrecise.find(object); pt != StorePrecise.end()) {
        for (auto &[start, storeAndEnd] : pt->second) {
          if (off.first == start && off.second == storeAndEnd.second) {
            shadowed = true;
          }
        }
      }
      if (!shadowed) {
        for (auto &off : offsets) {
          LoadPrecise[object].insert(off);
        }
      }
    }
  }
  LoadImprecise.Union(imprecise);
  for (auto it = LoadPrecise.begin(); it != LoadPrecise.end(); ) {
    if (LoadImprecise.Contains(it->first)) {
      LoadPrecise.erase(it++);
    } else {
      ++it;
    }
  }

  for (auto it = KillableStores.begin(); it != KillableStores.end(); ) {
    if (imprecise.Contains(it->first)) {
      KillableStores.erase(it++);
    } else {
      if (auto pt = precise.find(it->first); pt != precise.end()) {
        for (auto et = it->second.begin(); et != it->second.end(); ) {
          auto etStart = et->first;
          auto etEnd = et->second.first;
          for (auto [start, end] : pt->second) {
            llvm_unreachable("not implemented");
          }
        }
      }
      if (it->second.empty()) {
        KillableStores.erase(it++);
      } else {
        ++it;
      }
    }
  }
}

// -----------------------------------------------------------------------------
void ReverseNodeState::dump(llvm::raw_ostream &os)
{
  os << "\tLoad: " << LoadImprecise << "\n";
  for (auto &[id, loads] : LoadPrecise) {
    for (auto &[start, end] : loads) {
      os << "\t\t" << id << " + " << start << "," << end << "\n";
    }
  }
  os << "\tStore: " << StoreImprecise << "\n";
  for (auto &[id, stores] : StorePrecise) {
    for (auto &[off, storeAndEnd] : stores) {
      auto &[store, end] = storeAndEnd;
      os << "\t\t" << id << " + " << off << "," << end;
      if (store) {
        os << ": " << *store->GetAddr();
      }
      os << "\n";
    }
  }
  os << "\tKillable: \n";
  for (auto &[id, stores] : KillableStores) {
    for (auto &[off, storeAndEnd] : stores) {
      auto &[store, end] = storeAndEnd;
      os << "\t\t" << id << " + " << off << "," << end;
      if (store) {
        os << ": " << *store->GetAddr();
      }
      os << "\n";
    }
  }
}
