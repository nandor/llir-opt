  .text
mul_u32_const:
  .args            i32
  .call            c

  arg.i32          $0, 0
  mov.i32          $1, 13
  mul.i32          $2, $0, $1
  ret.i32          $2

mul_i32_const:
  .args            i32
  .call            c

  arg.i32          $0, 0
  mov.i32          $1, 13
  mul.i32          $2, $0, $1
  ret.i32          $2

rem_u32_const:
  .args            i32
  .call            c

  arg.i32          $0, 0
  mov.i32          $1, 13
  urem.i32          $2, $0, $1
  ret.i32          $2

rem_i32_const:
  .args            i32
  .call            c

  arg.i32          $0, 0
  mov.i32          $1, 13
  srem.i32          $2, $0, $1
  ret.i32          $2
