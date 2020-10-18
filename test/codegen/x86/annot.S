  .section .text
camlLexgen__anon_fn.5b.FILE.2flex.2flexgen.2eml.3a1161.2c6.2d.2d865.5d_3836:
  .visibility      global_hidden
  .call            caml
  .args            i64, i64, i64, i64

  arg.i64          $1001, 0
  arg.i64          $1009, 1
  arg.i64          $1017, 2
  arg.i64          $1025, 3
  mov.v64          $0, $1001
  mov.v64          $1, $1009
  mov.v64          $2, $1017
  mov.v64          $3, $1025
  mov.i64          $9, 24
  add.i64          $10, $0, $9
  ld.v64           $4, [$10]
  mov.v64          $1000, $4
  mov.i64          $11, camlLexgen__extract_tags_3627
  call.v64.caml   $1002, $11, $1000 @caml_frame
  mov.v64          $4, $1002
  mov.i64          $12, 16
  add.i64          $13, $0, $12
  ld.v64           $5, [$13]
  mov.v64          $1000, $5
  mov.i64          $14, camlLexgen__reset_state_partial_2126
  call.v64.caml   $1002, $14, $1000 @caml_frame
  mov.v64          $5, $1002
  mov.i64          $15, 8
  add.i64          $16, $0, $15
  ld.v64           $5, [$16]
  mov.v64          $1000, $5
  mov.i64          $17, camlLexgen__firstpos_1555
  call.v64.caml   $1002, $17, $1000 @caml_frame
  mov.v64          $5, $1002
  mov.v64          $1000, $5
  mov.i64          $18, camlLexgen__create_init_state_2504
  call.v64.caml   $1002, $18, $1000 @caml_frame
  mov.v64          $5, $1002
  mov.v64          $1000, $5
  mov.i64          $19, camlLexgen__get_state_2843
  call.v64.caml   $1002, $19, $1000 @caml_frame
  mov.v64          $5, $1002
  mov.i64          $20, 40
  add.i64          $21, $3, $20
  ld.v64           $6, [$21]
  ld.v64           $6, [$6]
  mov.i64          $22, 56
  mov.i64          $23, caml_allocN
  call.i64.caml_alloc  $7, $23, $22 @caml_frame
  mov.i64          $8, 0x18f7
  mov.i64          $24, -8
  add.i64          $25, $7, $24
  st               [$25], $8
  mov.v64          $8, camlLexgen__translate_state_8632
  st               [$7], $8
  mov.i64          $8, 0x3
  mov.i64          $26, 8
  add.i64          $27, $7, $26
  st               [$27], $8
  mov.i64          $28, 16
  add.i64          $29, $7, $28
  st               [$29], $2
  mov.i64          $30, 24
  add.i64          $31, $7, $30
  st               [$31], $4
  mov.i64          $32, 24
  add.i64          $33, $3, $32
  ld.v64           $2, [$33]
  mov.i64          $34, 32
  add.i64          $35, $7, $34
  st               [$35], $2
  mov.i64          $36, 32
  add.i64          $37, $3, $36
  ld.v64           $2, [$37]
  mov.i64          $38, 40
  add.i64          $39, $7, $38
  st               [$39], $2
  mov.v64          $1000, $7
  mov.v64          $1008, $6
  mov.i64          $40, camlLexgen__map_on_all_states_2991
  call.v64.caml   $1002, $40, $1000, $1008 @caml_frame
  mov.v64          $2, $1002
  mov.i64          $41, 40
  add.i64          $42, $3, $41
  ld.v64           $3, [$42]
  mov.v64          $1000, $3
  mov.v64          $1008, $2
  mov.i64          $43, caml_modify
  call.c           $43, $1000, $1008
  mov.v64          $2, 0x1
  mov.i64          $44, 24
  add.i64          $45, $0, $44
  ld.v64           $2, [$45]
  mov.v64          $3, camlLexgen__Pmakeblock_4141
  ld.v64           $3, [$3]
  ld.v64           $3, [$3]
  mov.i64          $46, 1
  cmp.eq.i32       $47, $3, $46
  jt               $47, .L717.63
  mov.v64          $3, camlLexgen__Pmakeblock_4142
  ld.v64           $3, [$3]
  ld.v64           $3, [$3]
  mov.i64          $48, 2
  add.i64          $3, $3, $48
  jmp              .L716.63
.L717.63:
  mov.v64          $3, camlLexgen__Pmakeblock_4142
  ld.v64           $3, [$3]
  ld.v64           $3, [$3]
.L716.63:
  mov.i64          $49, 48
  mov.i64          $50, caml_allocN
  call.i64.caml_alloc $4, $50, $49 @caml_frame
  mov.i64          $6, 0x1400
  mov.i64          $51, -8
  add.i64          $52, $4, $51
  st               [$52], $6
  ld.v64           $0, [$0]
  st               [$4], $0
  mov.i64          $53, 8
  add.i64          $54, $4, $53
  st               [$54], $1
  mov.i64          $55, 16
  add.i64          $56, $4, $55
  st               [$56], $3
  mov.i64          $57, 24
  add.i64          $58, $4, $57
  st               [$58], $5
  mov.i64          $59, 32
  add.i64          $60, $4, $59
  st               [$60], $2
  mov.v64          $0, camlLexgen__const_block_3804
  mov.v64          $1, 0x1
  mov.v64          $2, camlStdlib
  mov.i64          $61, 216
  add.i64          $62, $2, $61
  ld.v64           $2, [$62]
  mov.v64          $3, camlStdlib__printf__anon_fn.5b.FILE.2fstdlib.2fprintf.2eml.3a20.2c14.2d.2d50.5d_261_closure
  mov.v64          $1000, $3
  mov.v64          $1008, $2
  mov.v64          $1016, $1
  mov.v64          $1024, $0
  mov.i64          $63, camlCamlinternalFormat__make_printf_4904
  call.v64.caml   $1002, $63, $1000, $1008, $1016, $1024 @caml_frame
  mov.v64          $0, $1002
  mov.i64          $64, 1
  sll.i64          $1, $5, $64
  mov.i64          $65, 1
  add.i64          $1, $1, $65
  mov.i64          $66, 1
  sll.i64          $2, $4, $66
  mov.i64          $67, 1
  add.i64          $2, $2, $67
  mov.i64          $1000, $2
  mov.i64          $1008, $1
  mov.v64          $1016, $0
  mov.i64          $68, caml_apply2
  call.v64.caml   $1002, $68, $1000, $1008, $1016 @caml_frame
  mov.v64          $0, $1002
  mov.v64          $0, 0x1
  mov.v64          $1000, $0
  mov.i64          $69, camlStdlib__flush_all_805
  call.v64.caml   $1002, $69, $1000 @caml_frame
  mov.v64          $0, $1002
  mov.v64          $1002, $4
  ret.i64          $1002
