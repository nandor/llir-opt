  .code
_fext_f32_f64:
  .args        0, f32
  .call        c
  arg.f32      $0, 0
  fext.f64     $1, $0
  ret.f64      $1

_zext_i32_f32:
  .args        0, i32
  .call        c
  arg.i32      $0, 0
  zext.f32     $1, $0
  ret.f32      $1

_zext_i32_f64:
  .args        0, i32
  .call        c
  arg.i32      $0, 0
  zext.f64     $1, $0
  ret.f64      $1

_zext_i32_i64:
  .args        0, i32
  .call        c
  arg.i32      $0, 0
  zext.i64     $1, $0
  ret.i64      $1

_zext_i64_f32:
  .args        0, i64
  .call        c
  arg.i64      $0, 0
  zext.f32     $1, $0
  ret.f32      $1

_zext_i64_f64:
  .args        0, i64
  .call        c
  arg.i64      $0, 0
  zext.f64     $1, $0
  ret.f64      $1


_sext_i32_f32:
  .args        0, i32
  .call        c
  arg.i32      $0, 0
  sext.f32     $1, $0
  ret.f32      $1

_sext_i32_f64:
  .args        0, i32
  .call        c
  arg.i32      $0, 0
  sext.f64     $1, $0
  ret.f64      $1

_sext_i32_i64:
  .args        0, i32
  .call        c
  arg.i32      $0, 0
  sext.i64     $1, $0
  ret.i64      $1

_sext_i64_f32:
  .args        0, i64
  .call        c
  arg.i64      $0, 0
  sext.f32     $1, $0
  ret.f32      $1

_sext_i64_f64:
  .args        0, i64
  .call        c
  arg.i64      $0, 0
  sext.f64     $1, $0
  ret.f64      $1
