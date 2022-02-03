# RUN: %opt - -triple aarch64

  .section .text
  .globl longjmp
longjmp:
  .noinline
  .args               i64, i64
  .call               c

  arg.i64             $0, 0
  arg.i64             $1, 1

  mov.i64             $2, 0
  mov.i64             $3, 1
  select.i64          $4, $1, $2, $3
  add.i64             $5, $1, $4

  load.i64              $6, [$0]

  mov.i64             $7, 8
  add.i64             $8, $0, $7
  load.i64            $9, [$8]

  raise.setjmp        $9, $6, $5
