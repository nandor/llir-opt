camlTranslprim__primitive_is_ccall_2970:
  .visibility extern
  .call caml
  .args   i64
.LBBentry81:
  arg.i64 $0, 0 @caml_value
  mov.i64 $1, $0 @caml_value
  mov.i64 $2, 1
  and.i64 $3, $1, $2
  jcc $3, .LBBterm82, .L178
.L178:
  mov.i64 $5, -8
  add.i64 $6, $1, $5
  ld.i8 $7, $6
  zext.i64  $8, $7
  mov.i64 $9, -8
  add.i64 $10, $8, $9
  mov.i64 $11, 15
  cmp.ule.i8  $12, $10, $11
  jcc $12, .L177, .LBBterm86
.LBBterm86:
  mov.i64 $14, -32
  add.i64 $15, $10, $14
  mov.i64 $16, 1
  cmp.ule.i8  $17, $15, $16
  undef.i64 $18
  undef.i8  $19
  jcc $17, .L175, .LBBterm87
.LBBterm87:
  undef.i64 $21
  undef.i8  $22
  jmp .L176
.L177:
  mov.i64 $24, -2
  add.i64 $25, $10, $24
  mov.i64 $26, 11
  cmp.ugt.i8  $27, $25, $26
  undef.i64 $28
  undef.i8  $29
  jcc $27, .L175, .L176
.LBBterm82:
  mov.i64 $31, 1
  sra.i64 $32, $1, $31
  mov.i64 $33, 6
  cmp.lt.i8 $34, $32, $33
  jcc $34, .L179, .LBBterm83
.LBBterm83:
  mov.i64 $36, 30
  cmp.lt.i8 $37, $32, $36
  undef.i64 $38
  undef.i8  $39
  jcc $37, .L176, .LBBterm84
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
  jcc $47, .L175, .LBBterm85
.LBBterm85:
  undef.i64 $51
  undef.i8  $52
  jmp .L176
.L176:
  mov.i64 $54, 1 @caml_value
  mov.i64 $55, $54 @caml_value
  ret $55
.L175:
  mov.i64 $57, 3 @caml_value
  mov.i64 $58, $57 @caml_value
  ret $58
