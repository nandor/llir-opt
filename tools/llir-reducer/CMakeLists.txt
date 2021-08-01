# This file if part of the llir-opt project.
# Licensing information can be found in the LICENSE file.
# (C) 2018 Nandor Licker. All rights reserved.

# llir-reducer executable.
add_executable(llir-reducer
    reducer.cpp
    prog_reducer.cpp
    timeout.cpp
)
target_link_libraries(llir-reducer
    passes
    llir-core
    llir-analysis
    llir-adt
    ${LLVM_LIBS}
)
install(
    TARGETS llir-reducer
    DESTINATION bin
)

