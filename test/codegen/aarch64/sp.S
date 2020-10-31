# RUN: %opt - -triple aarch64

  .section .text
  .extern _DYNAMIC, ".text"
  .weak _DYNAMIC
  .hidden _DYNAMIC
  .globl _start
_start:
   mov.i64 $3, $sp
   mov.i64 $4, 8
   add.i64 $5, $3, $4
   mov.i64 $3, -16
   and.i64 $4, $5, $3
   set.i64 $sp, $4
   trap
   .end
