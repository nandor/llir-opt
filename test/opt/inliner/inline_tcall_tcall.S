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
  # CHECK: tcall.1.c

  mov.i64     $1, callee_tcall_tcall_void
  tcall.c     $1, $0
  .end
