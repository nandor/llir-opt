# This file if part of the genm-opt project.
# Licensing information can be found in the LICENSE file.
# (C) 2018 Nandor Licker. All rights reserved.

add_library(passes
    pta/graph.cpp
    pta/node.cpp
    pta/scc.cpp
    pta/solver.cpp

    dead_code_elim.cpp
    dead_func_elim.cpp
    higher_order.cpp
    inliner.cpp
    move_elim.cpp
    pta.cpp
    sccp.cpp
    simplify_cfg.cpp
)
