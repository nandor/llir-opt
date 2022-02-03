# RUN: %opt - -triple powerpc64le

feclearexcept:
  .visibility global_default
  .call c
  .args i32
.LBBentry1$local1625:
  .visibility local
  arg.i32 $0, 0
  mov.i32 $1, 1040187392
  and.i32 $2, $0, $1
  mov.i32 $3, 33031936
  or.i32  $4, $2, $3
  mov.i32 $5, 536870912
  and.i32 $6, $0, $5
  mov.i32 $7, 0
  cmp.eq.i8 $8, $6, $7
  select.i32  $9, $8, $2, $4
  mov.i32 $10, -1
  xor.i32 $11, $9, $10
  s_ext.i64  $12, $11
  get.f64 $13, $ppc_fpscr
  bit_cast.i64 $14, $13
  and.i64 $15, $14, $12
  bit_cast.f64 $16, $15
  set $ppc_fpscr, $16
  ret $7
  .end
