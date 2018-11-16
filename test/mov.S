  .text
_mov_f32_i32:
  .args        1
  .call        c
  arg.f32      $0, 0
  mov.i32      $1, $0
  ret.i32      $1

_mov_i32_f32:
  .args        1
  .call        c
  arg.i32      $0, 0
  mov.f32      $1, $0
  ret.f32      $1

_mov_f64_i64:
  .args        1
  .call        c
  arg.f64      $0, 0
  mov.i64      $1, $0
  ret.i64      $1

_mov_i64_f64:
  .args        1
  .call        c
  arg.i64      $0, 0
  mov.f64      $1, $0
  ret.f64      $1

_mov_i32_i32:
  .args        1
  .call        c
  arg.i32      $0, 0
  mov.i32      $1, $0
  ret.i32      $1

_mov_i64_i64:
  .args        1
  .call        c
  arg.i64      $0, 0
  mov.i64      $1, $0
  ret.i64      $1

_mov_f32_f32:
  .args        1
  .call        c
  arg.f32      $0, 0
  mov.f32      $1, $0
  ret.f32      $1

_mov_f64_f64:
  .args        1
  .call        c
  arg.f64      $0, 0
  mov.f64      $1, $0
  ret.f64      $1
