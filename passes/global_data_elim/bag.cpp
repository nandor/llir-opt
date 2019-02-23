// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.


#include "passes/global_data_elim/bag.h"
#include "passes/global_data_elim/heap.h"



// -----------------------------------------------------------------------------
void Bag::Item::Load(std::function<void(const Item&)> &&f) const
{
  switch (kind_) {
    case Kind::FUNC: {
      // Code of functions shouldn't be read.
      break;
    }
    case Kind::EXT: {
      // A bit iffy - might break things.
      break;
    }
    case Kind::NODE: {
      nodeVal_->Load(std::forward<decltype(f)>(f));
      break;
    }
  }
}

// -----------------------------------------------------------------------------
bool Bag::Item::Store(const Item &item) const
{
  switch (kind_) {
    case Kind::FUNC: {
      // Functions shouldn't be mutated.
      return false;
    }
    case Kind::EXT: {
      // External vars shouldn't be written.
      return false;
    }
    case Kind::NODE: {
      return nodeVal_->Store(item);
    }
  }
}

// -----------------------------------------------------------------------------
std::optional<Bag::Item> Bag::Item::Offset(const std::optional<int64_t> &off) const
{
  switch (kind_) {
    case Kind::FUNC: {
      return std::nullopt;
    }
    case Kind::EXT: {
      return std::nullopt;
    }
    case Kind::NODE: {
      return *this;
    }
  }
}

// -----------------------------------------------------------------------------
bool Bag::Store(const Item &item)
{
  switch (item.kind_) {
    case Item::Kind::FUNC: {
      return funcs_.insert(item.funcVal_).second;
    }
    case Item::Kind::EXT: {
      return exts_.insert(item.extVal_).second;
    }
    case Item::Kind::NODE: {
      return nodes_.emplace(item.nodeVal_).second;
    }
  }
}
