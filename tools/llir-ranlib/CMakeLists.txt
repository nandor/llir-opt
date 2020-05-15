# This file if part of the llir-opt project.
# Licensing information can be found in the LICENSE file.
# (C) 2018 Nandor Licker. All rights reserved.

# llir-ranlib executable.
add_executable(llir-ranlib ranlib.cpp)
target_link_libraries(llir-ranlib
    ${LLVM_LIBS}
)
install(
    TARGETS llir-ranlib
    DESTINATION bin
)
