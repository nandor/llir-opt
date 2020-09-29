  .section .text
_mov_f32_i32:
  .args        f32
  .call        c
  arg.f32      $0, 0
  mov.i32      $1, $0
  ret.i32      $1

_mov_i32_f32:
  .args        i32
  .call        c
  arg.i32      $0, 0
  mov.f32      $1, $0
  ret.f32      $1

_mov_f64_i64:
  .args        f64
  .call        c
  arg.f64      $0, 0
  mov.i64      $1, $0
  ret.i64      $1

_mov_i64_f64:
  .args        i64
  .call        c
  arg.i64      $0, 0
  mov.f64      $1, $0
  ret.f64      $1

_mov_i32_i32:
  .args        i32
  .call        c
  arg.i32      $0, 0
  mov.i32      $1, $0
  ret.i32      $1

_mov_i64_i64:
  .args        i64
  .call        c
  arg.i64      $0, 0
  mov.i64      $1, $0
  ret.i64      $1

_mov_f32_f32:
  .args        f32
  .call        c
  arg.f32      $0, 0
  mov.f32      $1, $0
  ret.f32      $1

_mov_f64_f64:
  .args        f64
  .call        c
  arg.f64      $0, 0
  mov.f64      $1, $0
  ret.f64      $1
  .section .text

_mov_i32:
  mov.i32      $0, 0xC2F63EFA
  ret.i32      $0

_mov_i64:
  mov.i64      $0, 0xC05EC7DF3B645A1D
  ret.i64      $0

_mov_f32:
  mov.f32      $0, 0xC2F63EFA
  ret.f32      $0

_mov_f64:
  mov.f64      $0, 0xC05EC7DF3B645A1D
  ret.f64      $0
