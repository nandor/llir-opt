  # RUN: %opt - -triple=powerpc64le

  .weak _DYNAMIC
  .hidden _DYNAMIC
  .section .text
_start:
  .visibility global_default
  .call c
  .args
.LBBentry1:
  .visibility local
  mov.i64 $0, $sp
  mov.i64 $1, -32
  and.i64 $2, $0, $1
  set $sp, $2
  mov.i64 $3, _DYNAMIC
  mov.i64 $4, _start_c
  # CHECK: _start_c
  tcall.c $4, $2, $3
  .end
