# RUN: %opt - -emit=llir -passes=inliner

  .section .text
callee_tcall_tcall_void:
  .args       i64
  .call       c
.Lentry_loop:
  mov.i64     $0, foo
  arg.i64     $1, 0
  tcall.c     $0, $1
  .end

caller_tcall_tcall_void:
  .visibility global_default
  .call       c

  mov.i64     $0, 0
  # CHECK: foo
  # CHECK: tail_call.c

  mov.i64     $1, callee_tcall_tcall_void
  tcall.c     $1, $0
  .end

caller_tcall_tcall_vararg_void:
  .visibility global_default
  .call       c

  mov.i64     $0, 0
  # CHECK: foo
  # CHECK: tail_call.c

  mov.i64       $20, 0
  mov.i64       $21, 1
  mov.i64       $1, callee_tcall_tcall_void
  tcall.1.c     $1, $0, $20, $21
  .end
