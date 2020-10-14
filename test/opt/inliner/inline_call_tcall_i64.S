# ARGS: -emit=llir -passes=inliner,dead-code-elim,dead-func-elim

  .section .text
callee_call_tcall_i64:
  .args       i64
  .call       c
.Lentry_loop:
  mov.i64     $0, foo
  arg.i64     $1, 0
  tcall.c.i64 $0, $1
  .end

caller_call_tcall_i64:
  .visibility global_default
  .call       c

  mov.i64     $0, 0
  mov.i64     $1, callee_call_tcall_i64
  call.c.i64  $2, $1, $0, .Lcont
.Lcont:
  mov.i64     $3, 1
  add.i64     $4, $2, $3
  ret         $4
  .end
