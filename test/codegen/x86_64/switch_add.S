# RUN: %opt - -triple x86_64
camlTranslprim__primitive_is_ccall_2970:
  .visibility global_default
  .call caml
  .args   v64
.LBBentry81:
  arg.v64 $0, 0
  mov.v64 $1, $0
  mov.i64 $2, 1
  and.i64 $3, $1, $2
  jump_cond $3, .LBBterm82, .L178
.L178:
  mov.i64 $5, -8
  add.i64 $6, $1, $5
  load.i8 $7, $6
  z_ext.i64  $8, $7
  mov.i64 $9, -8
  add.i64 $10, $8, $9
  mov.i64 $11, 15
  cmp.ule.i8  $12, $10, $11
  jump_cond $12, .L177, .LBBterm86
.LBBterm86:
  mov.i64 $14, -32
  add.i64 $15, $10, $14
  mov.i64 $16, 1
  cmp.ule.i8  $17, $15, $16
  undef.i64 $18
  undef.i8  $19
  jump_cond $17, .L175, .LBBterm87
.LBBterm87:
  undef.i64 $21
  undef.i8  $22
  jump .L176
.L177:
  mov.i64 $24, -2
  add.i64 $25, $10, $24
  mov.i64 $26, 11
  cmp.ugt.i8  $27, $25, $26
  undef.i64 $28
  undef.i8  $29
  jump_cond $27, .L175, .L176
.LBBterm82:
  mov.i64 $31, 1
  sra.i64 $32, $1, $31
  mov.i64 $33, 6
  cmp.lt.i8 $34, $32, $33
  jump_cond $34, .L179, .LBBterm83
.LBBterm83:
  mov.i64 $36, 30
  cmp.lt.i8 $37, $32, $36
  undef.i64 $38
  undef.i8  $39
  jump_cond $37, .L176, .LBBterm84
.LBBterm84:
  mov.i64 $41, -30
  add.i64 $42, $32, $41
  undef.i64 $43
  undef.i8  $44
  switch  $42, .L175, .L176, .L176, .L176, .L175, .L175, .L176, .L176, .L176, .L176, .L176
.L179:
  mov.i64 $46, 4
  cmp.ge.i8 $47, $32, $46
  undef.i64 $48
  undef.i8  $49
  jump_cond $47, .L175, .LBBterm85
.LBBterm85:
  undef.i64 $51
  undef.i8  $52
  jump .L176
.L176:
  mov.v64 $54, 1
  mov.v64 $55, $54
  ret $55
.L175:
  mov.v64 $57, 3
  mov.v64 $58, $57
  ret $58
