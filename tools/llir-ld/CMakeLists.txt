# This file if part of the llir-opt project.
# Licensing information can be found in the LICENSE file.
# (C) 2018 Nandor Licker. All rights reserved.

## llir-ld executable.
add_executable(llir-ld ld.cpp)
target_link_libraries(llir-ld
    core
    analysis
    adt
    ${LLVM_LIBS}
)
install(
    TARGETS llir-ld
    DESTINATION bin
)
