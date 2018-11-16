  .text
_trunc_f32_u32:
  .args        1
  .call        c
  arg.f32      $0, 0
  trunc.u32    $1, $0
  ret.u32      $1

_trunc_f32_i32:
  .args        1
  .call        c
  arg.f32      $0, 0
  trunc.i32    $1, $0
  ret.i32      $1

_trunc_f32_i64:
  .args        1
  .call        c
  arg.f32      $0, 0
  trunc.i64    $1, $0
  ret.i64      $1

_trunc_f64_u32:
  .args        1
  .call        c
  arg.f64      $0, 0
  trunc.u32    $1, $0
  ret.u32      $1

_trunc_f64_i32:
  .args        1
  .call        c
  arg.f64      $0, 0
  trunc.i32    $1, $0
  ret.i32      $1

_trunc_f64_i64:
  .args        1
  .call        c
  arg.f64      $0, 0
  trunc.i64    $1, $0
  ret.i64      $1

_trunc_i64_i32:
  .args        1
  .call        c
  arg.i64      $0, 0
  trunc.i64    $1, $0
  ret.i64      $1
