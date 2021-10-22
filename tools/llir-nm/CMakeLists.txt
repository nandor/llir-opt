# This file if part of the llir-opt project.
# Licensing information can be found in the LICENSE file.
# (C) 2018 Nandor Licker. All rights reserved.

## llir-nm executable.
add_executable(llir-nm nm.cpp)
target_link_libraries(llir-nm
    llir-core
    llir-analysis
    llir-adt
    ${LLVM_LIBS}
)
install(
    TARGETS llir-nm
    DESTINATION bin
)
