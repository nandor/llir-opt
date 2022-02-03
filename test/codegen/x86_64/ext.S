# RUN: %opt - -triple x86_64

  .section .text
_f_ext_f32_f64:
  .args        f32
  .call        c
  arg.f32      $0, 0
  f_ext.f64     $1, $0
  ret.f64      $1
  .end

_z_ext_i32_f32:
  .args        i32
  .call        c
  arg.i32      $0, 0
  z_ext.f32     $1, $0
  ret.f32      $1
  .end

_z_ext_i32_f64:
  .args        i32
  .call        c
  arg.i32      $0, 0
  z_ext.f64     $1, $0
  ret.f64      $1
  .end

_z_ext_i32_i64:
  .args        i32
  .call        c
  arg.i32      $0, 0
  z_ext.i64     $1, $0
  ret.i64      $1
  .end

_z_ext_i64_f32:
  .args        i64
  .call        c
  arg.i64      $0, 0
  z_ext.f32     $1, $0
  ret.f32      $1
  .end

_z_ext_i64_f64:
  .args        i64
  .call        c
  arg.i64      $0, 0
  z_ext.f64     $1, $0
  ret.f64      $1
  .end


_s_ext_i32_f32:
  .args        i32
  .call        c
  arg.i32      $0, 0
  s_ext.f32     $1, $0
  ret.f32      $1
  .end

_s_ext_i32_f64:
  .args        i32
  .call        c
  arg.i32      $0, 0
  s_ext.f64     $1, $0
  ret.f64      $1
  .end

_s_ext_i32_i64:
  .args        i32
  .call        c
  arg.i32      $0, 0
  s_ext.i64     $1, $0
  ret.i64      $1
  .end

_s_ext_i64_f32:
  .args        i64
  .call        c
  arg.i64      $0, 0
  s_ext.f32     $1, $0
  ret.f32      $1
  .end

_s_ext_i64_f64:
  .args        i64
  .call        c
  arg.i64      $0, 0
  s_ext.f64     $1, $0
  ret.f64      $1
  .end
