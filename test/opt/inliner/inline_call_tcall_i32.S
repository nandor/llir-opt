# RUN: %opt - -emit=llir -pass=inliner

  .section .text
callee_call_tcall_i32:
  .args       i64
  .call       c
.Lentry_loop:
  mov.i64     $0, foo
  arg.i64     $1, 0
  tcall.c.i32  $0, $1
  .end

caller_call_tcall_i32:
  .visibility global_default
  .call       c

  mov.i64     $0, 0
  mov.i64     $1, callee_call_tcall_i32
  call.c.i64  $2, $1, $0
  mov.i64     $3, 1
  # CHECK: x_ext
  add.i64     $4, $2, $3
  ret         $4
  .end
