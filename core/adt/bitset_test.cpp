// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/adt/bitset.h"
#include <gtest/gtest.h>


namespace {

TEST(BitsetSet, Iterate) {
  BitSet<unsigned> set;
  set.Insert(3662);
  set.Insert(3667);
  set.Insert(3670);
  set.Insert(3679);
  set.Insert(3688);
  set.Insert(3695);
  set.Insert(3701);
  set.Insert(3708);
  set.Insert(3709);
  set.Insert(3710);
  set.Insert(3712);

  BitSet<unsigned>::iterator it = set.begin();
  EXPECT_EQ(3662, *it++);
  EXPECT_EQ(3667, *it++);
  EXPECT_EQ(3670, *it++);
  EXPECT_EQ(3679, *it++);
  EXPECT_EQ(3688, *it++);
  EXPECT_EQ(3695, *it++);
  EXPECT_EQ(3701, *it++);
  EXPECT_EQ(3708, *it++);
  EXPECT_EQ(3709, *it++);
  EXPECT_EQ(3710, *it++);
  EXPECT_EQ(3712, *it++);
  EXPECT_EQ(set.end(), it);
}

TEST(BitsetSet, Erase) {
  BitSet<unsigned> set;
  set.Insert(1);
  set.Insert(2);
  set.Insert(5);
  set.Insert(6);
  set.Insert(128);
  set.Insert(129);
  set.Insert(200);
  set.Insert(220);

  set.Erase(1);
  set.Erase(2);
  set.Erase(5);
  set.Erase(6);
  set.Erase(129);
  set.Erase(220);

  BitSet<unsigned>::iterator it = set.begin();
  EXPECT_EQ(128, *it++);
  EXPECT_EQ(200, *it++);
  EXPECT_EQ(set.end(), it);
}


TEST(BitsetSet, Contains) {
  BitSet<unsigned> set;
  set.Insert(1);
  set.Insert(2);
  set.Insert(5);
  set.Insert(6);
  set.Insert(128);
  set.Insert(129);
  set.Insert(200);
  set.Insert(220);

  set.Erase(1);
  set.Erase(2);
  set.Erase(5);
  set.Erase(6);
  set.Erase(129);
  set.Erase(220);

  EXPECT_TRUE(set.Contains(128));
  EXPECT_TRUE(set.Contains(200));
}
}
