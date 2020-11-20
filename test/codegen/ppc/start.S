# RUN: %opt - -triple powerpc64le

  .section .text
  .extern _DYNAMIC, ".text"
  .weak _DYNAMIC
  .hidden _DYNAMIC
  .globl start
start:
  mov.i64 $1, _DYNAMIC
  mov.i64 $2, start_c
  mov.i64 $3, $sp
  mov.i64 $4, 32
  add.i64 $5, $3, $4
  mov.i64 $3, -16
  and.i64 $4, $5, $3
  set.i64 $sp, $4
  call.c  $2, $5, $1
  trap
  .end

