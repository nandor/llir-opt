# RUN: %opt - -triple x86_64
  .section .text
mul_u32_const:
  .args            i32
  .call            c

  arg.i32          $0, 0
  mov.i32          $1, 13
  mul.i32          $2, $0, $1
  ret.i32          $2
  .end

mul_i32_const:
  .args            i32
  .call            c

  arg.i32          $0, 0
  mov.i32          $1, 13
  mul.i32          $2, $0, $1
  ret.i32          $2
  .end

rem_u32_const:
  .args            i32
  .call            c

  arg.i32          $0, 0
  mov.i32          $1, 13
  u_rem.i32          $2, $0, $1
  ret.i32          $2
  .end

rem_i32_const:
  .args            i32
  .call            c

  arg.i32          $0, 0
  mov.i32          $1, 13
  s_rem.i32          $2, $0, $1
  ret.i32          $2
  .end
