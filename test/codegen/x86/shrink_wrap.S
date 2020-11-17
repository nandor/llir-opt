# RUN: %opt - -triple x86_64

caml_apply2:
  .visibility global_default
  .call caml
  .args i64,i64,v64,v64,v64
.LBBentry697:
  .visibility local
  arg.i64 $0, 0
  arg.i64 $1, 1
  arg.v64 $2, 2
  arg.v64 $3, 3
  arg.v64 $4, 4
  mov.i64 $5, 8
  add.i64 $6, $4, $5
  ld.v64  $7, $6
  mov.i64 $8, 5
  cmp.ne.i8 $9, $7, $8
  jcc $9, .L838, .LBBterm698 @probability(99 1000)
.LBBterm698:
  .visibility local
  mov.i64 $10, 16
  add.i64 $11, $4, $10
  ld.v64  $12, $11
  tcall.caml.i64.i64.v64  $12, $0, $1, $2, $3, $4 @caml_frame(() ())
.L838:
  .visibility local
  ld.v64  $13, $4
  call.caml.i64.i64.v64 $14, $15, $16, $13, $0, $1, $2, $4, .LBBterm699 @caml_frame(() ())
.LBBterm699:
  .visibility local
  ld.v64  $17, $16
  tcall.caml.i64.i64.v64  $17, $14, $15, $3, $16 @caml_frame(() ())
  .end
