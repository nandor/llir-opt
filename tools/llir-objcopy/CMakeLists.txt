# This file if part of the llir-opt project.
# Licensing information can be found in the LICENSE file.
# (C) 2018 Nandor Licker. All rights reserved.

## llir-objcopy executable.
add_executable(llir-objcopy objcopy.cpp objcopy.cpp)
target_link_libraries(llir-objcopy
    core
    analysis
    adt
    ${LLVM_LIBS}
)
install(
    TARGETS llir-objcopy
    DESTINATION bin
)
