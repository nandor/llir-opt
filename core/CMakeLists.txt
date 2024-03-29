# This file if part of the llir-opt project.
# Licensing information can be found in the LICENSE file.
# (C) 2018 Nandor Licker. All rights reserved.

add_subdirectory(adt)

add_custom_command(
    OUTPUT
        ${CMAKE_BINARY_DIR}/instructions.def
    COMMAND
        llir-tblgen
        ${CMAKE_CURRENT_SOURCE_DIR}/instructions.td
        -o ${CMAKE_BINARY_DIR}/instructions.def
    MAIN_DEPENDENCY
        ${CMAKE_CURRENT_SOURCE_DIR}/instructions.td
    DEPENDS
        llir-tblgen
)
set_property(
    DIRECTORY
        "${COMPONENT_PATH}"
    APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES
        ${CMAKE_BINARY_DIR}/instructions.def
)

add_library(llir-core
    ${CMAKE_BINARY_DIR}/instructions.def
    analysis.cpp
    annot.cpp
    atom.cpp
    bitcode_reader.cpp
    bitcode_writer.cpp
    block.cpp
    calling_conv.cpp
    cfg.cpp
    clone.cpp
    cond.cpp
    constant.cpp
    dag.cpp
    data.cpp
    error.cpp
    expr.cpp
    extern.cpp
    func.cpp
    global.cpp
    inst.cpp
    inst_compare.cpp
    insts.cpp
    insts/mov.cpp
    insts/phi.cpp
    item.cpp
    lexer.cpp
    object.cpp
    parser.cpp
    parser_inst.cpp
    parser_phi.cpp
    pass.cpp
    pass_manager.cpp
    pass_registry.cpp
    printer.cpp
    prog.cpp
    ref.cpp
    register.cpp
    symbol_table.cpp
    target.cpp
    target/aarch64.cpp
    target/ppc.cpp
    target/riscv.cpp
    target/x86.cpp
    type.cpp
    use.cpp
    user.cpp
    util.cpp
    value.cpp
    verifier.cpp
    visibility.cpp
    xtor.cpp
)
add_dependencies(llir-core llir-adt)

add_subdirectory(analysis)

install(TARGETS llir-core)
install(TARGETS llir-adt)
install(TARGETS llir-analysis)

install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DESTINATION include/llir
    FILES_MATCHING PATTERN "*.h"
)
install(
    FILES ${CMAKE_BINARY_DIR}/instructions.def
    DESTINATION ${CMAKE_INSTALL_PREFIX}/include/llir/core
)
