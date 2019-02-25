# This file if part of the genm-opt project.
# Licensing information can be found in the LICENSE file.
# (C) 2018 Nandor Licker. All rights reserved.

add_library(passes
    global_data_elim/bitset.cpp
    global_data_elim/node.cpp
    global_data_elim/scc.cpp
    global_data_elim/solver.cpp

    dead_code_elim.cpp
    dead_func_elim.cpp
    global_data_elim.cpp
    higher_order.cpp
    inliner.cpp
    move_elim.cpp
    sccp.cpp
    simplify_cfg.cpp
)
