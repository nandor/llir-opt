// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/adt/bitset.h"
#include <gtest/gtest.h>


namespace {

TEST(BitsetTest, IterateForward) {
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

TEST(BitsetTest, IterateForwardMany) {
  BitSet<unsigned> set;
  for (unsigned i = 1000; i < 10000; ++i) {
    set.Insert(i);
  }

  BitSet<unsigned>::iterator it = set.begin();
  for (unsigned i = 1000; i < 10000; ++i) {
    EXPECT_EQ(i, *it++);
  }
  EXPECT_EQ(set.end(), it);
}

TEST(BitsetTest, IterateReverse) {
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

  BitSet<unsigned>::reverse_iterator it = set.rbegin();
  EXPECT_EQ(3712, *it++);
  EXPECT_EQ(3710, *it++);
  EXPECT_EQ(3709, *it++);
  EXPECT_EQ(3708, *it++);
  EXPECT_EQ(3701, *it++);
  EXPECT_EQ(3695, *it++);
  EXPECT_EQ(3688, *it++);
  EXPECT_EQ(3679, *it++);
  EXPECT_EQ(3670, *it++);
  EXPECT_EQ(3667, *it++);
  EXPECT_EQ(3662, *it++);
  EXPECT_EQ(set.rend(), it);
}

TEST(BitsetTest, Erase) {
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


TEST(BitsetTest, Contains) {
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

TEST(BitsetTest, Union) {
  BitSet<unsigned> a;
  a.Insert(1);
  a.Insert(2);
  a.Insert(5);
  a.Insert(6);
  BitSet<unsigned> b;
  b.Insert(5);
  b.Insert(8);
  b.Insert(7);
  b.Insert(9);

  EXPECT_EQ(3, a.Union(b));
  EXPECT_TRUE(a.Contains(1));
  EXPECT_TRUE(a.Contains(2));
  EXPECT_TRUE(a.Contains(5));
  EXPECT_TRUE(a.Contains(6));
  EXPECT_TRUE(a.Contains(7));
  EXPECT_TRUE(a.Contains(9));
}

TEST(BitsetTest, Equal) {
  BitSet<unsigned> a;
  a.Insert(1);
  a.Insert(2);
  a.Insert(5);
  a.Insert(6);
  BitSet<unsigned> b;
  b.Insert(1);
  b.Insert(2);
  b.Insert(5);
  b.Insert(6);

  EXPECT_TRUE(a == b);
}

TEST(BitsetTest, IterateSparseForward) {
  BitSet<unsigned> set;
  set.Insert(30);
  set.Insert(60);
  set.Insert(80);
  set.Insert(200);
  set.Insert(1000);

  BitSet<unsigned>::iterator it = set.begin();
  EXPECT_EQ(30, *it++);
  EXPECT_EQ(60, *it++);
  EXPECT_EQ(80, *it++);
  EXPECT_EQ(200, *it++);
  EXPECT_EQ(1000, *it++);
  EXPECT_EQ(set.end(), it);
}

TEST(BitsetTest, IterateSparseReverse) {
  BitSet<unsigned> set;
  set.Insert(30);
  set.Insert(60);
  set.Insert(80);
  set.Insert(200);
  set.Insert(1000);

  BitSet<unsigned>::reverse_iterator rt = set.rbegin();
  EXPECT_EQ(1000, *rt++);
  EXPECT_EQ(200, *rt++);
  EXPECT_EQ(80, *rt++);
  EXPECT_EQ(60, *rt++);
  EXPECT_EQ(30, *rt++);
  EXPECT_EQ(set.rend(), rt);
}

}
