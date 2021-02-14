# RUN: %opt - -emit=llir -pass=inliner -pass=dead-code-elim -pass=dead-func-elim

  .section .text
callee_call_tcall_i64:
  .args           i64
  .call           c
.Lentry_loop:
  mov.i64         $0, foo
  arg.i64         $1, 0
  tcall.c.i64.i64 $0, $1
  .end

caller_call_tcall_i64:
  .visibility     global_default
  .call           c

  mov.i64         $0, 0
  mov.i64         $1, callee_call_tcall_i64
  call.c.i64.i64  $2, $3, $1, $0, .Lcont
.Lcont:
  add.i64         $5, $2, $3
  ret             $5
  .end
