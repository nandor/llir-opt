# RUN: %opt - -triple riscv64 -mcpu sifive-u74

  .section .text
  .extern _DYNAMIC, ".text"
  .weak _DYNAMIC
  .hidden _DYNAMIC
  .globl start
start:
  # CHECK: .option push
  # CHECK: .option norelax
  # CHECK: .option pop
  riscv_gp
  mov.i64 $1, _DYNAMIC
  mov.i64 $2, start_c
  mov.i64 $3, $sp
  mov.i64 $4, 16
  add.i64 $5, $3, $4
  mov.i64 $3, -16
  and.i64 $4, $5, $3
  set.i64 $sp, $4
  call.c  $2, $5, $1
  trap
  .end

