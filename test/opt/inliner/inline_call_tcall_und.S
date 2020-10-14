# ARGS: -emit=llir -passes=inliner

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
  # CHECK: undef
  mov.i64     $1, callee_tcall_tcall_void
  call.c.i64  $2, $1, $0
  mov.i64     $3, 2
  add.i64     $4, $2, $3
  ret.i64     $4
  .end
