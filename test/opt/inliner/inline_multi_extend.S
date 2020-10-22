# ARGS: -emit=llir -passes=inliner,dead-code-elim,dead-func-elim

  .section .text
callee_call_tcall_i64:
  .args               i64
  .call               c
.Lentry_loop:
  mov.i64             $0, foo
  arg.i64             $1, 0
  tcall.c.i64.i32.i8  $0, $1
  .end

caller_call_tcall_i64:
  .visibility     global_default
  .call           c

  mov.i64                 $0, 0
  mov.i64                 $1, callee_call_tcall_i64
  call.c.i64.i64.i64.i64  $10, $11, $12, $13, $1, $0, .Lcont
.Lcont:
  add.i64                 $20, $10, $11
  add.i64                 $21, $12, $13
  add.i64                 $22, $20, $21
  ret                     $22
  .end
