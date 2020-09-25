# This file if part of the llir-opt project.
# Licensing information can be found in the LICENSE file.
# (C) 2018 Nandor Licker. All rights reserved.

## llir-dump executable.
add_executable(llir-dump dump.cpp)
target_link_libraries(llir-dump
    core
    analysis
    adt
    ${LLVM_LIBS}
)
install(
    TARGETS llir-dump
    DESTINATION bin
)
