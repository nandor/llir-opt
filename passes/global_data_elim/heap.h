// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <map>
#include <unordered_map>

class Atom;



/**
 * Base class of nodes modelling the heap.
 */
class Node {
public:
  Node()
  {
  }

  void Load(std::function<void(const Bag::Item&)> &&f)
  {
    bag_.ForEach(std::forward<decltype(f)>(f));
  }

  bool Store(const Bag::Item &item)
  {
    return bag_.Store(item);
  }

private:
  /// Underlying bag.
  Bag bag_;
};
