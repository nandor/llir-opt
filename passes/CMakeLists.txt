# This file if part of the llir-opt project.
# Licensing information can be found in the LICENSE file.
# (C) 2018 Nandor Licker. All rights reserved.

add_library(passes
    inliner/inline_helper.cpp
    inliner/trampoline_graph.cpp

    pta/graph.cpp
    pta/node.cpp
    pta/scc.cpp
    pta/solver.cpp

    local_const/analysis.cpp
    local_const/builder.cpp
    local_const/context.cpp
    local_const/graph.cpp
    local_const/scc.cpp

    sccp/eval.cpp
    sccp/lattice.cpp

    vtpta/constraint.cpp
    vtpta/builder.cpp

    dead_code_elim.cpp
    dead_func_elim.cpp
    higher_order.cpp
    inliner.cpp
    local_const.cpp
    move_elim.cpp
    pta.cpp
    reduce.cpp
    sccp.cpp
    simplify_cfg.cpp
    simplify_trampoline.cpp
    tail_rec_elim.cpp
    vtpta.cpp
)
