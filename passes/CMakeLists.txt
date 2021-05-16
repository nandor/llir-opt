# This file if part of the llir-opt project.
# Licensing information can be found in the LICENSE file.
# (C) 2018 Nandor Licker. All rights reserved.

add_library(passes
    global_forward/forwarder.cpp
    global_forward/nodes.cpp
    global_forward/reverse.cpp
    global_forward/transfer.cpp

    inliner/inline_helper.cpp
    inliner/inline_util.cpp
    inliner/trampoline_graph.cpp

    pta/graph.cpp
    pta/node.cpp
    pta/scc.cpp
    pta/solver.cpp

    pre_eval/eval/arith.cpp
    pre_eval/eval/bitwise.cpp
    pre_eval/eval/cmp.cpp
    pre_eval/eval/const.cpp
    pre_eval/eval/conv.cpp
    pre_eval/eval/hardware.cpp
    pre_eval/eval/memory.cpp
    pre_eval/eval/shift.cpp
    pre_eval/eval/x86.cpp
    pre_eval/pointer_closure.cpp
    pre_eval/symbolic_approx.cpp
    pre_eval/symbolic_context.cpp
    pre_eval/symbolic_eval.cpp
    pre_eval/symbolic_frame.cpp
    pre_eval/symbolic_heap.cpp
    pre_eval/symbolic_object.cpp
    pre_eval/symbolic_pointer.cpp
    pre_eval/symbolic_summary.cpp
    pre_eval/symbolic_value.cpp

    sccp/compare.cpp
    sccp/eval.cpp
    sccp/lattice.cpp
    sccp/load.cpp
    sccp/solver.cpp
    sccp/x86.cpp

    tags/analysis.cpp
    tags/init.cpp
    tags/refinement.cpp
    tags/step.cpp
    tags/tagged_type.cpp
    tags/eval/add.cpp
    tags/eval/sub.cpp
    tags/eval/and.cpp
    tags/eval/xor.cpp
    tags/eval/or.cpp
    tags/eval/conv.cpp
    tags/eval/shift.cpp

    atom_simplify.cpp
    bypass_phi.cpp
    caml_alloc_inliner.cpp
    caml_assign.cpp
    caml_global_simplify.cpp
    code_layout.cpp
    cond_simplify.cpp
    const_global.cpp
    dead_code_elim.cpp
    dead_data_elim.cpp
    dead_func_elim.cpp
    dead_store.cpp
    dedup_block.cpp
    eliminate_select.cpp
    eliminate_tags.cpp
    global_forward.cpp
    inliner.cpp
    libc_simplify.cpp
    linearise.cpp
    link.cpp
    localize_select.cpp
    mem_to_reg.cpp
    merge_stores.cpp
    move_elim.cpp
    object_split.cpp
    peephole.cpp
    phi_taut.cpp
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
    unused_arg.cpp
    value_numbering.cpp
    verifier.cpp
)
add_dependencies(passes core)
