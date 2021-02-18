# This file if part of the llir-opt project.
# Licensing information can be found in the LICENSE file.
# (C) 2018 Nandor Licker. All rights reserved.

## generate the helper to parse command line options.
add_custom_command(
    OUTPUT
        ${CMAKE_BINARY_DIR}/LdOptions.inc
    COMMAND
        llvm-tblgen
          ${CMAKE_CURRENT_SOURCE_DIR}/LdOptions.td
          -o ${CMAKE_BINARY_DIR}/LdOptions.inc
          -I ${LLVM_INCLUDE_DIRS}
          -gen-opt-parser-defs
    MAIN_DEPENDENCY
        ${CMAKE_CURRENT_SOURCE_DIR}/LdOptions.td
    DEPENDS
        llvm-tblgen
)
set_property(
    DIRECTORY
        "${COMPONENT_PATH}"
    APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES
        ${CMAKE_BINARY_DIR}/LdOptions.inc
)

## llir-ld executable.
add_executable(llir-ld
    ${CMAKE_BINARY_DIR}/LdOptions.inc
    driver.cpp
    ld.cpp
    linker.cpp
    options.cpp
)
target_link_libraries(llir-ld
    core
    analysis
    adt
    ${LLVM_LIBS}
    ${LLVM_LTO_LIBS}
)
install(
    TARGETS llir-ld
    DESTINATION bin
)
