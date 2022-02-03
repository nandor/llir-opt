# RUN: %opt - -triple x86_64
xmm_call:
  .args            i64, i64, i64, i64
  .call            c
  arg.i64          $1, 0
  arg.i64          $2, 1
  arg.i64          $3, 2
  arg.i64          $4, 3

  load.8.f64         $21, [$4]
  trunc.f32        $22, $21
  f_ext.f64         $23, $22
  load.8.f64         $24, [$3]
  trunc.f32        $25, $24
  f_ext.f64         $26, $25
  load.8.f64         $27, [$2]
  trunc.f32        $28, $27
  f_ext.f64         $29, $28
  load.8.f64         $30, [$1]
  trunc.f32        $31, $30
  f_ext.f64         $32, $31

  mov.i64          $33, L.str.28
  mov.i64          $34, xmm_callee
  call.i32.1.c     $35, $33, $34, $32, $29, $26, $23
  ret.i32          $35
  .end

