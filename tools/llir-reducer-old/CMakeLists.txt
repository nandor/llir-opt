# This file if part of the llir-opt project.
# Licensing information can be found in the LICENSE file.
# (C) 2018 Nandor Licker. All rights reserved.

# llir-reducer-old executable
add_executable(llir-reduce reduce_old.cpp reduce_pass.cpp)
target_link_libraries(llir-reduce
    passes
    core
    analysis
    ${LLVM_LIBS}
)
install(
    TARGETS llir-reduce
    DESTINATION bin
)
install(
    PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/llir-reducer-old
    DESTINATION bin
)

