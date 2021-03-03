# RUN: %opt - -triple x86_64


  .section .text
  .globl starter
starter:
  .noinline
  .args i64, i64

  arg.i64 $0, 0
  arg.i64 $1, 1
  # CHECK: movq
  # CHECK: jmpq
  spawn   $0, $1
  .end
