// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <gtest/gtest.h>

#include "core/adt/union_find.h"



namespace {

class Item {
public:
  Item(ID<Item> id, unsigned key)
  {
    keys_.insert(key);
  }

  void Union(const Item &that)
  {
    std::copy(
        that.keys_.begin(),
        that.keys_.end(),
        std::inserter(keys_, keys_.begin())
    );
  }

private:
  std::set<unsigned> keys_;
};

TEST(UnionFindTest, Test) {
  UnionFind<Item> items_;
  auto id0 = items_.Emplace(0);
  auto id1 = items_.Emplace(1);
  auto id2 = items_.Emplace(2);
  auto id3 = items_.Emplace(3);
  auto id4 = items_.Emplace(4);

  items_.Union(id0, id3);
  items_.Union(id3, id4);
  items_.Union(id1, id2);
}

}
