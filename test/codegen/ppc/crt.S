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
  get.i64 $1, $sp
  mov.i64 $2, -32
  and.i64 $3, $1, $2
  mov.i64 $4, 32
  sub.i64 $5, $3, $4
  set.i64 $sp, $5
  mov.i64 $6, _DYNAMIC
  mov.i64 $7, _start_c
  tcall.c $7, $1, $6
  .end



_start_c:
  .visibility local
  trap
  .end
