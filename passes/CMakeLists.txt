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

    pre_eval/symbolic_context.cpp
    pre_eval/symbolic_eval.cpp
    pre_eval/symbolic_value.cpp
    pre_eval/tainted_atoms.cpp

    local_const/builder.cpp
    local_const/context.cpp
    local_const/graph.cpp
    local_const/scc.cpp
    local_const/store_elimination.cpp
    local_const/store_propagation.cpp

    sccp/eval.cpp
    sccp/lattice.cpp

    vtpta/constraint.cpp
    vtpta/builder.cpp

    dead_code_elim.cpp
    dead_data_elim.cpp
    dead_func_elim.cpp
    dedup_block.cpp
    higher_order.cpp
    inliner.cpp
    local_const.cpp
    move_elim.cpp
    pre_eval.cpp
    pta.cpp
    rewriter.cpp
    sccp.cpp
    simplify_cfg.cpp
    simplify_trampoline.cpp
    stack_object_elim.cpp
    tail_rec_elim.cpp
    undef_elim.cpp
    verifier.cpp
    vtpta.cpp
)
