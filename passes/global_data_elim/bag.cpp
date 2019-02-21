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
      if (off_) {
        nodeVal_->Load(*off_, std::forward<std::function<void(const Item&)>>(f));
      } else {
        nodeVal_->Load(std::forward<std::function<void(const Item&)>>(f));
      }
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
      if (off_) {
        return nodeVal_->Store(*off_, item);
      } else {
        return nodeVal_->Store(item);
      }
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
      if (auto size = nodeVal_->GetSize()) {
        if (off_ && off) {
          auto newOff = *off_ + *off;
          if (0 <= newOff && newOff < size) {
            return std::optional<Item>(std::in_place, nodeVal_, *off_ + *off);
          }
        }
      }
      return std::optional<Item>(std::in_place, nodeVal_);
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
      if (nodes_.find(item.nodeVal_) != nodes_.end()) {
        return false;
      }

      if (item.off_) {
        return offs_.emplace(item.nodeVal_, *item.off_).second;
      } else {
        return nodes_.emplace(item.nodeVal_).second;
      }
    }
  }
}
