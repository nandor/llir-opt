# RUN: %opt - -triple riscv64 -mcpu sifive-u74 -mabi lp64d



test:
  .args i64, i64, i64, v64, i64
  .call caml


  arg.i64 $0, 0
  arg.i64 $1, 1
  arg.i64 $2, 2

  arg.v64 $3, 3
  arg.i64 $4, 4

  mov.i64     $5, calee_c
  call.c.i64  $6, $5

  add.i64     $7, $3, $4
  ret         $0, $1, $2, $7


