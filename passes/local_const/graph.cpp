// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "passes/local_const/graph.h"



// -----------------------------------------------------------------------------
LCAlloc::LCAlloc(
    LCGraph *graph,
    ID<LCAlloc> id,
    const std::optional<uint64_t> &size,
    const uint64_t maxSize)
  : graph_(graph)
  , id_(id)
  , allocSize_(size)
  , size_(size ? std::min(*size, maxSize) : maxSize)
  , nodeIn_(graph_->Set()->GetID())
  , nodeOut_(graph_->Set()->GetID())
  , bucket_(graph_->Set()->GetID())
{
  auto *set = graph_->Find(bucket_);
  graph_->Find(nodeIn_)->Edge(set);
  set->Edge(graph_->Find(nodeOut_));
}

// -----------------------------------------------------------------------------
std::optional<LCIndex> LCAlloc::Offset(LCIndex index, int64_t offset)
{
  switch (index) {
    case kPositive: {
      // If index is moving back into object, return range.
      return offset > 0 ? std::optional<LCIndex>(index) : std::nullopt;
    }
    case kInvalid: {
      // If index is invalid, it stays invalid, exploiting UB.
      return LCIndex(kInvalid);
    }
    default: {
      if (offset < 0) {
        if (-offset <= index) {
          // New index definitely in bounds - return it.
          return LCIndex(index + offset);
        } else {
          // Negative indices are invalid for pointer arithmetic.
          return LCIndex(kInvalid);
        }
      } else if (offset > 0) {
        if (offset == size_ - index) {
          if (allocSize_ && *allocSize_ == size_) {
            // Full object, one-past-end pointer.
            return LCIndex(size_);
          } else {
            // One-past-end of object of unknown size or partial object.
            return LCIndex(kPositive);
          }
        } else if (offset < size_ - index) {
          // New offset in bounds.
          return LCIndex(index + offset);
        } else {
          // New offset out of bounds.
          if (allocSize_ && *allocSize_ == size_) {
            // Invalid if object fully modelled.
            return LCIndex(kInvalid);
          } else {
            // Special bucket otherwise.
            return LCIndex(kPositive);
          }
        }
      } else {
        return LCIndex(index);
      }
    }
  }
}

// -----------------------------------------------------------------------------
std::optional<ID<LCSet>> LCAlloc::GetElement(LCIndex index)
{
  switch (index) {
    case kInvalid: {
      // Negatives always map to invalid objects.
      return std::nullopt;
    }
    case kPositive: {
      // Map to the bucket containing all out-of-bounds or unmodelled elements.
      return bucket_;
    }
    default: {
      if (allocSize_ && *allocSize_ == size_ && *allocSize_ == index) {
        // One-past-end, nothing to read.
        return std::nullopt;
      }
      assert(index < size_ && "invalid element index");
      if (index % 8 != 0) {
        // Unaligned pointer - nothing to read.
        return std::nullopt;
      }

      // Create a bucket if one does not exist.
      uint64_t bucket = index / 8;
      auto it = elems_.find(bucket);
      if (it == elems_.end()) {
        auto *set = graph_->Set();
        graph_->Find(nodeIn_)->Edge(set);
        set->Edge(graph_->Find(nodeOut_));
        it = elems_.insert({bucket, set->GetID()}).first;
      }
      return it->second;
    }
  }
}

// -----------------------------------------------------------------------------
LCIndex LCAlloc::GetIndex(uint64_t index)
{
  if (allocSize_ && *allocSize_ == size_) {
    // Object of known size.
    return LCIndex(index <= size_ ? index : kInvalid);
  } else {
    // Object of unknown size.
    return LCIndex(index < size_ ? index : kPositive);
  }
}

// -----------------------------------------------------------------------------
LCDeref *LCSet::Deref()
{
  if (!deref_) {
    deref_.reset(new LCDeref(graph_, this));
  }
  return deref_.get();
}

// -----------------------------------------------------------------------------
bool LCSet::AddElement(LCAlloc *alloc, LCIndex elem)
{
  return pointsToElem_.insert({alloc->GetID(), elem}).second;
}

// -----------------------------------------------------------------------------
bool LCSet::AddRange(LCAlloc *alloc)
{
  return pointsToRange_.Insert(alloc->GetID());
}

// -----------------------------------------------------------------------------
bool LCSet::Edge(LCSet *set)
{
  return setOuts_.Insert(set->GetID());
}

// -----------------------------------------------------------------------------
bool LCSet::Edge(LCDeref *deref)
{
  if (derefOuts_.Insert(deref->set_->GetID())) {
    deref->setIns_.Insert(GetID());
    return true;
  }
  return false;
}

// -----------------------------------------------------------------------------
bool LCSet::Range(LCSet *set)
{
  return rangeOuts_.Insert(set->GetID());
}

// -----------------------------------------------------------------------------
bool LCSet::Offset(LCSet *set, int64_t offset)
{
  return offsetOuts_.insert({set->GetID(), offset}).second;
}

// -----------------------------------------------------------------------------
bool LCSet::Propagate(LCSet *to)
{
  bool changed = false;
  changed |= to->pointsToRange_.Union(pointsToRange_);
  for (auto elem : pointsToElem_) {
    if (to->pointsToElem_.insert(elem).second) {
      changed = true;
    }
  }
  return changed;
}

// -----------------------------------------------------------------------------
bool LCSet::Equals(LCSet *that)
{
  if (pointsToRange_ != that->pointsToRange_) {
    return false;
  }
  if (pointsToElem_ != that->pointsToElem_) {
    return false;
  }
  return true;
}

// -----------------------------------------------------------------------------
void LCSet::sets(std::function<void(LCSet *)> &&f)
{
  for (auto set : setOuts_) {
    f(graph_->Find(set));
  }
}

// -----------------------------------------------------------------------------
void LCSet::ranges(std::function<void(LCSet *)> &&f)
{
  for (auto set : rangeOuts_) {
    f(graph_->Find(set));
  }
}

// -----------------------------------------------------------------------------
void LCSet::offsets(std::function<void(LCSet *, int64_t)> &&f)
{
  for (auto [alloc, index] : offsetOuts_) {
    f(graph_->Find(alloc), index);
  }
}

// -----------------------------------------------------------------------------
void LCSet::deref_outs(std::function<void(LCDeref *)> &&f)
{
  for (auto deref : derefOuts_) {
    f(graph_->Find(deref)->Deref());
  }
}

// -----------------------------------------------------------------------------
void LCSet::points_to_set(std::function<void(ID<LCSet>, ID<LCSet>)> &&f)
{
  for (auto range : pointsToRange_) {
    LCAlloc *a = graph_->Find(range);
    f(a->GetNodeIn(), a->GetNodeOut());
  }

  for (auto it = pointsToElem_.begin(); it != pointsToElem_.end(); ) {
    if (pointsToRange_.Contains(it->first)) {
      pointsToElem_.erase(it++);
      continue;
    }

    if (auto set = graph_->Find(it->first)->GetElement(LCIndex(it->second))) {
      f(*set, *set);
    }
    ++it;
  }
}

// -----------------------------------------------------------------------------
void LCSet::points_to_range(std::function<void(LCAlloc *)> &&f)
{
  for (auto range : pointsToRange_) {
    f(graph_->Find(range));
  }
}

// -----------------------------------------------------------------------------
void LCSet::points_to_elem(std::function<void(LCAlloc *, LCIndex)> &&f)
{
  for (auto &[alloc, index] : pointsToElem_) {
    if (!pointsToRange_.Contains(alloc)) {
      f(graph_->Find(alloc), LCIndex(index));
    }
  }
}

// -----------------------------------------------------------------------------
bool LCDeref::Edge(LCSet *set)
{
  if (setOuts_.Insert(set->GetID())) {
    set->derefIns_.Insert(set_->GetID());
    return true;
  }
  return false;
}

// -----------------------------------------------------------------------------
void LCDeref::set_ins(std::function<void(LCSet *)> &&f)
{
  for (auto set : setIns_) {
    f(graph_->Find(set));
  }
}

// -----------------------------------------------------------------------------
void LCDeref::set_outs(std::function<void(LCSet *)> &&f)
{
  for (auto set : setOuts_) {
    f(graph_->Find(set));
  }
}


// -----------------------------------------------------------------------------
LCGraph::Entry::Entry(LCGraph *g, uint32_t id)
  : Parent(id)
  , Rank(0)
  , Set(new LCSet(g, id))
{
}

// -----------------------------------------------------------------------------
LCSet *LCGraph::Set()
{
  ID<LCSet> id(sets_.size());
  return sets_.emplace_back(this, id).Set.get();
}

// -----------------------------------------------------------------------------
LCAlloc *LCGraph::Alloc(const std::optional<uint64_t> &size, uint64_t maxSize)
{
  ID<LCAlloc> id(allocs_.size());
  auto *alloc = new LCAlloc(this, id, size, maxSize);
  allocs_.emplace_back(alloc);
  return alloc;
}

// -----------------------------------------------------------------------------
LCSet *LCGraph::Get(ID<LCSet> id)
{
  assert(id < sets_.size() && "invalid set ID");
  return sets_[id].Set.get();
}

// -----------------------------------------------------------------------------
LCSet *LCGraph::Find(ID<LCSet> id)
{
  uint32_t root = id;
  while (sets_[root].Parent != root) {
    root = sets_[root].Parent;
  }
  while (sets_[id].Parent != id) {
    uint32_t parent = sets_[id].Parent;
    sets_[id].Parent = root;
    id = parent;
  }
  return sets_[id].Set.get();
}

// -----------------------------------------------------------------------------
LCDeref *LCGraph::Find(ID<LCDeref> id)
{
  return Find(ID<LCSet>(static_cast<unsigned>(id)))->Deref();
}

// -----------------------------------------------------------------------------
LCAlloc *LCGraph::Find(ID<LCAlloc> id)
{
  assert(id < allocs_.size() && "invalid set ID");
  return allocs_[id].get();
}

// -----------------------------------------------------------------------------
void LCGraph::Replace(LCSet *a, LCSet *b)
{
  assert(a != b && "Attempting to replace pointer with self");

  b->setOuts_.Union(a->setOuts_);
  b->rangeOuts_.Union(a->rangeOuts_);
  b->derefIns_.Union(a->derefIns_);
  b->derefOuts_.Union(a->derefOuts_);
  for (auto &elem : a->pointsToElem_) {
    b->pointsToElem_.insert(elem);
  }

  if (a->deref_) {
    if (b->deref_) {
      Replace(a->deref_.get(), b->deref_.get());
    } else {
      b->deref_ = std::move(a->deref_);
      b->deref_->set_ = b;
    }
    a->deref_ = nullptr;
  }
}

// -----------------------------------------------------------------------------
void LCGraph::Replace(LCDeref *a, LCDeref *b)
{
  for (auto inID : a->setIns_) {
    auto *in = Find(inID);
    in->derefOuts_.Erase(a->set_->id_);
    in->derefOuts_.Insert(b->set_->id_);
    b->setIns_.Insert(inID);
  }

  for (auto outID : a->setOuts_) {
    auto *out = Find(outID);
    out->derefIns_.Erase(a->set_->id_);
    out->derefIns_.Insert(b->set_->id_);
    b->setOuts_.Insert(outID);
  }
}

// -----------------------------------------------------------------------------
ID<LCSet> LCGraph::Union(ID<LCSet> idA, ID<LCSet> idB)
{
  if (idA == idB) {
    return idB;
  }

  Entry &entryA = sets_[idA];
  Entry &entryB = sets_[idB];
  LCSet *a = entryA.Set.get();
  LCSet *b = entryB.Set.get();

  LCSet *node;
  if (entryA.Rank < entryB.Rank) {
    entryA.Parent = idB;
    a->Propagate(b);
    Replace(a, b);
    sets_[idA].Set = nullptr;
    node = b;
  } else {
    entryB.Parent = idA;
    b->Propagate(a);
    Replace(b, a);
    sets_[idB].Set = nullptr;
    node = a;
  }

  if (entryA.Rank == entryB.Rank) {
    entryA.Rank += 1;
  }
  return node->GetID();
}

