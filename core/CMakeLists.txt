# This file if part of the llir-opt project.
# Licensing information can be found in the LICENSE file.
# (C) 2018 Nandor Licker. All rights reserved.


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

add_library(core
    ${CMAKE_BINARY_DIR}/instructions.def
    analysis.cpp
    annot.cpp
    atom.cpp
    bitcode_reader.cpp
    bitcode_writer.cpp
    block.cpp
    call_graph.cpp
    cfg.cpp
    clone.cpp
    constant.cpp
    data.cpp
    expr.cpp
    extern.cpp
    func.cpp
    global.cpp
    inst.cpp
    insts/binary.cpp
    insts/call.cpp
    insts/const.cpp
    insts/control.cpp
    insts/hardware.cpp
    insts/hardware_x86.cpp
    insts/hardware_aarch64.cpp
    insts/hardware_ppc.cpp
    insts/hardware_riscv.cpp
    insts/memory.cpp
    insts/phi.cpp
    insts/ternary.cpp
    insts/unary.cpp
    item.cpp
    lexer.cpp
    object.cpp
    object_graph.cpp
    parser.cpp
    parser_inst.cpp
    parser_phi.cpp
    pass.cpp
    pass_manager.cpp
    pass_registry.cpp
    printer.cpp
    prog.cpp
    ref.cpp
    symbol_table.cpp
    type.cpp
    use.cpp
    user.cpp
    util.cpp
    value.cpp
    xtor.cpp
)

add_subdirectory(analysis)
add_subdirectory(adt)
