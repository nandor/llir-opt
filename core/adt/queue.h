// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <vector>



/**
 * Efficient work queue.
 */
template<typename T>
class Queue final {
public:
  /// Adds an item to the end of the queue.
  void Push(ID<T> item)
  {
    if (item >= dedup_.size()) {
      dedup_.resize(item + 1);
    }
    if (!dedup_[item]) {
      dedup_[item] = true;
      placeQ_.push_back(item);
    }
  }

  /// Pops an item from the queue.
  ID<T> Pop()
  {
    if (takeQ_.empty()) {
      takeQ_.reserve(placeQ_.size());
      for (size_t i = 0, n = placeQ_.size(); i < n; ++i) {
        takeQ_.push_back(placeQ_[n - i - 1]);
      }
      placeQ_.clear();
    }

    const auto &item = takeQ_.back();
    takeQ_.pop_back();
    dedup_[item] = false;
    return item;
  }

  /// Checks if the queue is empty.
  bool Empty() const
  {
    return takeQ_.empty() && placeQ_.empty();
  }

private:
  /// Queue to put items in.
  std::vector<ID<T>> placeQ_;
  /// Queue to take items from.
  std::vector<ID<T>> takeQ_;
  /// Hash set to dedup items.
  std::vector<bool> dedup_;
};
