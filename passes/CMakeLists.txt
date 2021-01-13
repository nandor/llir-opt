# This file if part of the llir-opt project.
# Licensing information can be found in the LICENSE file.
# (C) 2018 Nandor Licker. All rights reserved.

add_library(passes
    inliner/inline_helper.cpp
    inliner/inline_util.cpp
    inliner/trampoline_graph.cpp

    pta/graph.cpp
    pta/node.cpp
    pta/scc.cpp
    pta/solver.cpp

    pre_eval/eval/arith.cpp
    pre_eval/eval/conv.cpp
    pre_eval/eval/cmp.cpp
    pre_eval/eval/bitwise.cpp
    pre_eval/eval/shift.cpp
    pre_eval/eval/x86.cpp
    pre_eval/heap_graph.cpp
    pre_eval/symbolic_approx.cpp
    pre_eval/symbolic_context.cpp
    pre_eval/symbolic_eval.cpp
    pre_eval/symbolic_frame.cpp
    pre_eval/symbolic_loop.cpp
    pre_eval/symbolic_object.cpp
    pre_eval/symbolic_pointer.cpp
    pre_eval/symbolic_value.cpp

    local_const/builder.cpp
    local_const/context.cpp
    local_const/graph.cpp
    local_const/scc.cpp
    local_const/store_elimination.cpp
    local_const/store_propagation.cpp

    sccp/eval.cpp
    sccp/lattice.cpp

    atom_simplify.cpp
    caml_alloc_inliner.cpp
    caml_global_simplify.cpp
    cond_simplify.cpp
    const_global.cpp
    dead_code_elim.cpp
    dead_data_elim.cpp
    dead_func_elim.cpp
    dead_store.cpp
    dedup_block.cpp
    eliminate_select.cpp
    inliner.cpp
    link.cpp
    libc_simplify.cpp
    local_const.cpp
    mem_to_reg.cpp
    move_elim.cpp
    peephole.cpp
    pre_eval.cpp
    pta.cpp
    sccp.cpp
    simplify_cfg.cpp
    simplify_trampoline.cpp
    specialise.cpp
    stack_object_elim.cpp
    store_to_load.cpp
    tail_rec_elim.cpp
    undef_elim.cpp
    verifier.cpp
)
add_dependencies(passes core)
