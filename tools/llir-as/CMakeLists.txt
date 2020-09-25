# This file if part of the llir-opt project.
# Licensing information can be found in the LICENSE file.
# (C) 2018 Nandor Licker. All rights reserved.

## llir-as executable.
add_executable(llir-as as.cpp)
target_link_libraries(llir-as
    core
    analysis
    adt
    ${LLVM_LIBS}
)
install(
    TARGETS llir-as
    DESTINATION bin
)
