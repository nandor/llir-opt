# RUN: %opt - -triple x86_64

cgt_init:
  .visibility local
  .call c
  .args i32,i64
.LBBentry9:
  .visibility local
  arg.i64 $0, 1
  arg.i32 $1, 0
  mov.i64 $2, .L.str.1
  mov.i64 $3, .L.str
  mov.i64 $4, __vdsosym
  call.2.c.i64  $5, $4, $3, $2, .LBBterm10
.LBBterm10:
  .visibility local
  mov.i64 $6, vdso_func
  mov.i64 $7, cgt_init
  # cmpxchg
  x86_cmp_xchg.i64 $8, $6, $5, $7
  mov.i64 $9, 0
  cmp.eq.i8 $10, $5, $9
  jump_cond $10, .LBB1_2, .LBBterm11
.LBBterm11:
  .visibility local
  tcall.2.c.i32 $5, $1, $0
.LBB1_2:
  .visibility local
  mov.i32 $11, -38
  ret $11
  .end
