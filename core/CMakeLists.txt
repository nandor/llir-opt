# This file if part of the llir-opt project.
# Licensing information can be found in the LICENSE file.
# (C) 2018 Nandor Licker. All rights reserved.

add_library(core
    analysis.cpp
    annot.cpp
    atom.cpp
    bitcode.cpp
    block.cpp
    cfg.cpp
    call_graph.cpp
    clone.cpp
    constant.cpp
    data.cpp
    expr.cpp
    extern.cpp
    func.cpp
    global.cpp
    inst.cpp
    inst_visitor.cpp
    insts/binary.cpp
    insts/call.cpp
    insts/const.cpp
    insts/control.cpp
    insts/hardware.cpp
    insts/memory.cpp
    insts/unary.cpp
    insts/ternary.cpp
    insts/phi.cpp
    object.cpp
    parser.cpp
    pass.cpp
    pass_manager.cpp
    pass_registry.cpp
    printer.cpp
    prog.cpp
    symbol_table.cpp
    type.cpp
    util.cpp
    value.cpp
)

add_subdirectory(analysis)
add_subdirectory(adt)
