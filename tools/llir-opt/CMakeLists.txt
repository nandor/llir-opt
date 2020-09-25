# This file if part of the llir-opt project.
# Licensing information can be found in the LICENSE file.
# (C) 2018 Nandor Licker. All rights reserved.

# llir-opt executable.
add_executable(llir-opt opt.cpp)
target_link_libraries(llir-opt
    passes
    stats
    x86_emitter
    coq_emitter
    core
    adt
    analysis
    ${LLVM_LIBS}
)
install(
    TARGETS llir-opt
    DESTINATION bin
)
