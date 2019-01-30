  .code
mul_u32_const:
  .args            0, u32
  .call            c

  arg.u32          $0, 0
  mov.u32          $1, 13
  mul.u32          $2, $0, $1
  ret.u32          $2

mul_i32_const:
  .args            0, i32
  .call            c

  arg.i32          $0, 0
  mov.i32          $1, 13
  mul.i32          $2, $0, $1
  ret.i32          $2

rem_u32_const:
  .args            0, u32
  .call            c

  arg.u32          $0, 0
  mov.u32          $1, 13
  rem.u32          $2, $0, $1
  ret.u32          $2

rem_i32_const:
  .args            0, i32
  .call            c

  arg.i32          $0, 0
  mov.i32          $1, 13
  rem.i32          $2, $0, $1
  ret.i32          $2
