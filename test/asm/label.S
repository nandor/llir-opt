  .extern caml_greaterequal
  .extern caml_c_call_i64
  .extern caml_c_call_i64_i64
  .extern caml_create_bytes
  .extern caml_blit_bytes
  .extern caml_modify
  .extern camlStdlib__bytes__Pmakeblock_2692
  .extern caml_raise_exception

  .code
camlCamlinternalFormat__buffer_check_size_351:
  .call            caml
  .args            0, i64, i64

  arg.i64          $0, 0
  arg.i64          $1, 1
  mov.i64          $6, 8
  add.i64          $7, $0, $6
  ld.8.u64         $2, [$7]
  mov.i64          $8, -8
  add.i64          $9, $2, $8
  ld.8.u64         $3, [$9]
  mov.i64          $10, 10
  srl.i64          $3, $3, $10
  mov.i64          $11, 3
  sll.i64          $3, $3, $11
  mov.i64          $12, 1
  sub.i64          $3, $3, $12
  add.i64          $2, $2, $3
  ld.1.u64         $2, [$2]
  sub.i64          $2, $3, $2
  mov.i64          $13, 1
  sll.i64          $2, $2, $13
  mov.i64          $14, 1
  add.i64          $2, $2, $14
  ld.8.u64         $3, [$0]
  add.i64          $1, $3, $1
  mov.i64          $15, -1
  add.i64          $1, $1, $15
  cmp.le.i32       $16, $1, $2
  jt               $16, .L182.13
  mov.i64          $17, 1
  sll.i64          $3, $2, $17
  mov.i64          $18, -1
  add.i64          $3, $3, $18
  mov.i64          $19, caml_greaterequal
  mov.i64          $20, caml_c_call_i64_i64
  call.i64.caml_ext $4, $20, $19, $3, $1 @caml_frame
  mov.i64          $21, 1
  cmp.eq.i32       $22, $4, $21
  jt               $22, .L185.13
  mov.i64          $1, $3
.L185.13:
  mov.i64          $23, caml_create_bytes
  mov.i64          $24, caml_c_call_i64
  call.i64.caml_ext $1, $24, $23, $1 @caml_frame
  mov.i64          $25, 8
  add.i64          $26, $0, $25
  ld.8.u64         $3, [$26]
  mov.i64          $27, 1
  cmp.lt.i32       $28, $2, $27
  jt               $28, .L184.13
  mov.i64          $29, -8
  add.i64          $30, $3, $29
  ld.8.u64         $4, [$30]
  mov.i64          $31, 10
  srl.i64          $4, $4, $31
  mov.i64          $32, 3
  sll.i64          $4, $4, $32
  mov.i64          $33, 1
  sub.i64          $4, $4, $33
  add.i64          $5, $3, $4
  ld.1.u64         $5, [$5]
  sub.i64          $4, $4, $5
  mov.i64          $34, 1
  sll.i64          $4, $4, $34
  sub.i64          $4, $4, $2
  mov.i64          $35, 2
  add.i64          $4, $4, $35
  mov.i64          $36, 1
  cmp.lt.i32       $37, $4, $36
  jt               $37, .L184.13
  mov.i64          $38, -8
  add.i64          $39, $1, $38
  ld.8.u64         $4, [$39]
  mov.i64          $40, 10
  srl.i64          $4, $4, $40
  mov.i64          $41, 3
  sll.i64          $4, $4, $41
  mov.i64          $42, 1
  sub.i64          $4, $4, $42
  add.i64          $5, $1, $4
  ld.1.u64         $5, [$5]
  sub.i64          $4, $4, $5
  mov.i64          $43, 1
  sll.i64          $4, $4, $43
  sub.i64          $4, $4, $2
  mov.i64          $44, 2
  add.i64          $4, $4, $44
  mov.i64          $45, 1
  cmp.lt.i32       $46, $4, $45
  jt               $46, .L184.13
  mov.i64          $4, 1
  mov.i64          $5, 1
  mov.i64          $47, caml_blit_bytes
  call.i64.c       $2, $47, $3, $5, $1, $4, $2
  jmp              .L183.13
.L184.13:
  mov.i64          $0, camlStdlib__bytes__Pmakeblock_2692
  mov.i64          $48, caml_raise_exception
  tcall.c          $48, $0 @caml_frame
.L183.13:
  mov.i64          $49, 8
  add.i64          $0, $0, $49
  mov.i64          $50, caml_modify
  call.c           $50, $0, $1
  mov.i64          $0, 1
  ret.i64          $0
.L182.13:
  mov.i64          $0, 1
  ret.i64          $0
