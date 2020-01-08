# This file if part of the genm-opt project.
# Licensing information can be found in the LICENSE file.
# (C) 2018 Nandor Licker. All rights reserved.

add_executable(bitset_test bitset_test.cpp)
target_link_libraries(bitset_test ${GTEST_BOTH_LIBRARIES} pthread)
add_test(adt-tests bitset_test)
