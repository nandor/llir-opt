# This file if part of the llir-opt project.
# Licensing information can be found in the LICENSE file.
# (C) 2018 Nandor Licker. All rights reserved.

if (GTest_FOUND)
  add_executable(bitset_test bitset_test.cpp)
  target_link_libraries(bitset_test ${GTEST_BOTH_LIBRARIES} pthread)
  add_test(bitset_test bitset_test)

  add_executable(union_find_test union_find_test.cpp)
  target_link_libraries(union_find_test ${GTEST_BOTH_LIBRARIES} pthread)
  add_test(union_find_test union_find_test)
endif(GTest_FOUND)
