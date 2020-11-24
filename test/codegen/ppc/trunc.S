# RUN: %opt - -triple ppc64le

mt_generate_geom:
  .visibility local
  .call c
  .noinline
  .args   f64
.LBBentry97$local9448:
  .visibility local
  arg.f64   $1, 0
  trunc.f32 $7, $1
  mov.i64 $8, logf
  # CHECK: xsrsp
  call.c.f32  $9, $8, $7, .LBBterm100$local9451
.LBBterm100$local9451:
  .visibility local
  ret $9
  .end
