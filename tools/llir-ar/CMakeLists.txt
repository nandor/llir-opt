# This file if part of the llir-opt project.
# Licensing information can be found in the LICENSE file.
# (C) 2018 Nandor Licker. All rights reserved.

## llir-ar executable.
add_executable(llir-ar ar.cpp)
target_link_libraries(llir-ar
    llir-core
    llir-analysis
    llir-adt
    ${LLVM_LIBS}
)
install(
    TARGETS llir-ar
    DESTINATION bin
)
