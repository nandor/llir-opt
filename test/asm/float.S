  .text
_float_abs_f32:
  .args        0, f32
  .call        c
  arg.f32      $0, 0
  abs.f32      $1, $0
  ret.f32      $1

_float_neg_f32:
  .args        0, f32
  .call        c
  arg.f32      $0, 0
  neg.f32      $1, $0
  ret.f32      $1

_float_sqrt_f32:
  .args        0, f32
  .call        c
  arg.f32      $0, 0
  sqrt.f32      $1, $0
  ret.f32      $1

_float_sin_f32:
  .args        0, f32
  .call        c
  arg.f32      $0, 0
  sin.f32      $1, $0
  ret.f32      $1

_float_cos_f32:
  .args        0, f32
  .call        c
  arg.f32      $0, 0
  cos.f32      $1, $0
  ret.f32      $1

_float_abs_f64:
  .args        0, f64
  .call        c
  arg.f64      $0, 0
  abs.f64      $1, $0
  ret.f64      $1

_float_neg_f64:
  .args        0, f64
  .call        c
  arg.f64      $0, 0
  neg.f64      $1, $0
  ret.f64      $1

_float_sqrt_f64:
  .args        0, f64
  .call        c
  arg.f64      $0, 0
  sqrt.f64      $1, $0
  ret.f64      $1

_float_sin_f64:
  .args        0, f64
  .call        c
  arg.f64      $0, 0
  sin.f64      $1, $0
  ret.f64      $1

_float_cos_f64:
  .args        0, f64
  .call        c
  arg.f64      $0, 0
  cos.f64      $1, $0
  ret.f64      $1
