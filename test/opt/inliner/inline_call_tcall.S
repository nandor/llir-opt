# RUN: %opt - -emit=llir -pass=inliner -pass=sccp

  .section .text
callee_call_tcall_void:
  .args       i64
  .call       c
.Lentry_loop:
  mov.i64     $0, foo
  arg.i64     $1, 0
  mov.i64     $2, 1
  add.i64     $3, $2, $1
  tcall.c     $0, $3
  .end

caller_call_tcall_void:
  .visibility global_default
  .call       c

  mov.i64     $0, 2
  # CHECK: 3
  mov.i64     $1, callee_call_tcall_void
  # CHECK: .Lcont
  call.c      $1, $0, .Lcont
.Lcont:
  mov.i64     $2, 1
  ret         $2
  .end
