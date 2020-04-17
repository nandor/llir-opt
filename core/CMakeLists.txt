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
    clone.cpp
    constant.cpp
    data.cpp
    expr.cpp
    extern.cpp
    func.cpp
    global.cpp
    inst.cpp
    insts.cpp
    insts_binary.cpp
    insts_call.cpp
    insts_const.cpp
    insts_control.cpp
    insts_hardware.cpp
    insts_memory.cpp
    insts_unary.cpp
    parser.cpp
    pass.cpp
    pass_manager.cpp
    pass_registry.cpp
    printer.cpp
    prog.cpp
    type.cpp
    util.cpp
    value.cpp
)

add_subdirectory(analysis)
add_subdirectory(adt)
