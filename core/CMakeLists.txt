# This file if part of the genm-opt project.
# Licensing information can be found in the LICENSE file.
# (C) 2018 Nandor Licker. All rights reserved.

add_library(core
    atom.cpp
    block.cpp
    constant.cpp
    data.cpp
    dominator.cpp
    expr.cpp
    func.cpp
    global.cpp
    inst.cpp
    insts.cpp
    insts_binary.cpp
    insts_call.cpp
    insts_control.cpp
    insts_memory.cpp
    insts_unary.cpp
    parser.cpp
    pass.cpp
    pass_manager.cpp
    printer.cpp
    prog.cpp
    symbol.cpp
    type.cpp
    value.cpp
)
