
  .extern caml_local_roots
  .extern L.str.28
  .extern printf
  .extern simd_alloc.28
simd_make:
  .stack       96
  .args        0, i64, i64, i64, i64
  .call        c
  arg.i64          $1, 0
  arg.i64          $2, 1
  arg.i64          $3, 2
  arg.i64          $4, 3

  ld.8.f64         $21, [$4]
  trunc.f32        $22, $21
  fext.f64         $23, $22
  ld.8.f64         $24, [$3]
  trunc.f32        $25, $24
  fext.f64         $26, $25
  ld.8.f64         $27, [$2]
  trunc.f32        $28, $27
  fext.f64         $29, $28
  ld.8.f64         $30, [$1]
  trunc.f32        $31, $30
  fext.f64         $32, $31
  mov.i64          $33, L.str.28
  mov.i64          $34, printf
  call.i32.1.c     $35, $34, $33, $32, $29, $26, $23
  mov.i64          $36, simd_alloc.28
  call.i64.fast    $37, $36, $31, $28, $25, $22
  ret.i64          $37
