# RUN: %opt - -triple x86_64
  .section .text
umulo:
  .args     i32, i32

  arg.i32   $0, 0
  arg.i32   $1, 1
  o_u_mul.i32 $2, $0, $1
  ret.i32   $2
  .end

uaddo:
  .args     i32, i32

  arg.i32   $0, 0
  arg.i32   $1, 1
  o_u_add.i32 $2, $0, $1
  ret.i32   $2
  .end
