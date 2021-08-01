# This file if part of the llir-opt project.
# Licensing information can be found in the LICENSE file.
# (C) 2018 Nandor Licker. All rights reserved.


add_library(llir-adt
  sexp.cpp
)

if (GTest_FOUND)
  add_executable(bitset_test bitset_test.cpp)
  target_link_libraries(bitset_test
      ${GTEST_BOTH_LIBRARIES}
      ${LLVM_LIBS}
      pthread
  )
  add_test(bitset_test bitset_test)

  add_executable(union_find_test union_find_test.cpp)
  target_link_libraries(union_find_test
      ${GTEST_BOTH_LIBRARIES}
      ${LLVM_LIBS}
      pthread
  )
  add_test(union_find_test union_find_test)

  add_executable(sexp_test sexp_test.cpp)
  target_link_libraries(sexp_test
      ${GTEST_BOTH_LIBRARIES}
      pthread
      llir-adt
      ${LLVM_LIBS}
  )
  add_test(sexp_test sexp_test)
endif(GTest_FOUND)
