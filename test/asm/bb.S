.extern camlCamlinternalFormat__const_block_6732
.extern camlCamlinternalFormat__const_block_6868
.extern camlCamlinternalFormat__const_string_6714
.extern camlCamlinternalFormat__failwith_message_6667
.extern camlCamlinternalFormat__parse_int_6742
.extern camlCamlinternalFormat__parse_lword_6741
.extern camlCamlinternalFormat__parse_spaces_6740
.extern caml_string_equal
.extern caml_allocN
.extern caml_curry2
.extern caml_alloc2
.extern camlStdlib__bytes__sub_213
.extern caml_exception_pointer
.extern caml_int_of_string
.extern caml_c_call_i64
.extern caml_exn_val
.extern caml_exn_Failure
.extern caml_raise_exception
.extern caml_ml_array_bound_error

camlCamlinternalFormat__open_box_of_string_6709:
  .call            caml
  .stack           24
  .args            0, i64
  arg.i64          $1001, 0
  mov.i64          $0, $1001
  mov.i64          $1, camlCamlinternalFormat__const_string_6714
  mov.i64          $1000, $0
  mov.i64          $1008, $1
  mov.i64          $9, caml_string_equal
  call.i64.c       $1002, $9, $1000, $1008
  mov.i64          $1, $1002
  mov.i64          $10, 1
  cmp.eq.i32       $11, $1, $10
  jt               $11, .L1559.13
  mov.i64          $0, camlCamlinternalFormat__const_block_6868
  mov.i64          $1002, $0
  ret.i64          $1002
.L1559.13:
  mov.i64          $12, -8
  add.i64          $13, $0, $12
  ld.8.u64         $1, [$13]
  mov.i64          $14, 10
  srl.i64          $1, $1, $14
  mov.i64          $15, 3
  sll.i64          $1, $1, $15
  mov.i64          $16, 1
  sub.i64          $1, $1, $16
  add.i64          $2, $0, $1
  ld.1.u64         $2, [$2]
  sub.i64          $1, $1, $2
  mov.i64          $17, 1
  sll.i64          $1, $1, $17
  mov.i64          $18, 1
  add.i64          $1, $1, $18
  mov.i64          $19, 104
  mov.i64          $20, caml_allocN
  call.i64.c       $2, $20, $19 @caml_frame
  mov.i64          $3, 12535
  mov.i64          $21, -8
  add.i64          $22, $2, $21
  st.8             [$22], $3
  mov.i64          $3, camlCamlinternalFormat__parse_spaces_6740
  st.8             [$2], $3
  mov.i64          $3, 3
  mov.i64          $23, 8
  add.i64          $24, $2, $23
  st.8             [$24], $3
  mov.i64          $3, 3321
  mov.i64          $25, 16
  add.i64          $26, $2, $25
  st.8             [$26], $3
  mov.i64          $3, caml_curry2
  mov.i64          $27, 24
  add.i64          $28, $2, $27
  st.8             [$28], $3
  mov.i64          $4, 5
  mov.i64          $29, 32
  add.i64          $30, $2, $29
  st.8             [$30], $4
  mov.i64          $4, camlCamlinternalFormat__parse_lword_6741
  mov.i64          $31, 40
  add.i64          $32, $2, $31
  st.8             [$32], $4
  mov.i64          $4, 7417
  mov.i64          $33, 48
  add.i64          $34, $2, $33
  st.8             [$34], $4
  mov.i64          $35, 56
  add.i64          $36, $2, $35
  st.8             [$36], $3
  mov.i64          $3, 5
  mov.i64          $37, 64
  add.i64          $38, $2, $37
  st.8             [$38], $3
  mov.i64          $3, camlCamlinternalFormat__parse_int_6742
  mov.i64          $39, 72
  add.i64          $40, $2, $39
  st.8             [$40], $3
  mov.i64          $41, 80
  add.i64          $42, $2, $41
  st.8             [$42], $0
  mov.i64          $43, 88
  add.i64          $44, $2, $43
  st.8             [$44], $1
  mov.i64          $45, 1
  cmp.ne.i32       $46, $1, $45
  jt               $46, .L1558.13
  mov.i64          $3, 1
  jmp              .L1556.13
.L1558.13:
  mov.i64          $47, -8
  add.i64          $48, $0, $47
  ld.8.u64         $3, [$48]
  mov.i64          $49, 10
  srl.i64          $3, $3, $49
  mov.i64          $50, 3
  sll.i64          $3, $3, $50
  mov.i64          $51, 1
  sub.i64          $3, $3, $51
  add.i64          $4, $0, $3
  ld.1.u64         $4, [$4]
  sub.i64          $3, $3, $4
  mov.i64          $52, 0
  cmp.le.i32       $53, $3, $52
  jt               $53, .L1560.13
  ld.1.u64         $3, [$0]
  mov.i64          $54, 1
  sll.i64          $3, $3, $54
  mov.i64          $55, 1
  add.i64          $3, $3, $55
  mov.i64          $56, 19
  cmp.eq.i32       $57, $3, $56
  jt               $57, .L1557.13
  mov.i64          $58, 65
  cmp.eq.i32       $59, $3, $58
  jt               $59, .L1557.13
  mov.i64          $3, 1
  jmp              .L1556.13
.L1557.13:
  mov.i64          $3, 3
  mov.i64          $1000, $3
  mov.i64          $1008, $2
  mov.i64          $60, camlCamlinternalFormat__parse_spaces_6740
  call.i64.caml   $1002, $60, $1000, $1008 @caml_frame
  mov.i64          $3, $1002
.L1556.13:
  cmp.ne.i32       $61, $3, $1
  jt               $61, .L1555.13
  mov.i64          $4, $3
  jmp              .L1553.13
.L1555.13:
  mov.i64          $62, 1
  sra.i64          $4, $3, $62
  mov.i64          $63, -8
  add.i64          $64, $0, $63
  ld.8.u64         $5, [$64]
  mov.i64          $65, 10
  srl.i64          $5, $5, $65
  mov.i64          $66, 3
  sll.i64          $5, $5, $66
  mov.i64          $67, 1
  sub.i64          $5, $5, $67
  add.i64          $6, $0, $5
  ld.1.u64         $6, [$6]
  sub.i64          $5, $5, $6
  cmp.le.i32       $68, $5, $4
  jt               $68, .L1560.13
  add.i64          $4, $0, $4
  ld.1.u64         $4, [$4]
  mov.i64          $69, 1
  sll.i64          $4, $4, $69
  mov.i64          $70, -193
  add.i64          $4, $4, $70
  mov.i64          $71, 51
  cmp.ule.i32      $72, $4, $71
  jt               $72, .L1554.13
  mov.i64          $4, $3
  jmp              .L1553.13
.L1554.13:
  mov.i64          $73, 24
  add.i64          $4, $2, $73
  mov.i64          $74, 2
  add.i64          $5, $3, $74
  mov.i64          $1000, $3
  mov.i64          $1008, $5
  mov.i64          $1016, $4
  mov.i64          $75, camlCamlinternalFormat__parse_lword_6741
  call.i64.caml   $1002, $75, $1000, $1008, $1016 @caml_frame
  mov.i64          $4, $1002
.L1553.13:
  sub.i64          $5, $4, $3
  mov.i64          $76, 1
  add.i64          $5, $5, $76
  mov.i64          $1000, $0
  mov.i64          $1008, $3
  mov.i64          $1016, $5
  mov.i64          $77, camlStdlib__bytes__sub_213
  call.i64.caml   $1002, $77, $1000, $1008, $1016 @caml_frame
  mov.i64          $3, $1002
  cmp.ne.i32       $78, $4, $1
  jt               $78, .L1552.13
  jmp              .L1550.13
.L1552.13:
  mov.i64          $79, 1
  sra.i64          $5, $4, $79
  mov.i64          $80, -8
  add.i64          $81, $0, $80
  ld.8.u64         $6, [$81]
  mov.i64          $82, 10
  srl.i64          $6, $6, $82
  mov.i64          $83, 3
  sll.i64          $6, $6, $83
  mov.i64          $84, 1
  sub.i64          $6, $6, $84
  add.i64          $7, $0, $6
  ld.1.u64         $7, [$7]
  sub.i64          $6, $6, $7
  cmp.le.i32       $85, $6, $5
  jt               $85, .L1560.13
  add.i64          $5, $0, $5
  ld.1.u64         $5, [$5]
  mov.i64          $86, 1
  sll.i64          $5, $5, $86
  mov.i64          $87, 1
  add.i64          $5, $5, $87
  mov.i64          $88, 19
  cmp.eq.i32       $89, $5, $88
  jt               $89, .L1551.13
  mov.i64          $90, 65
  cmp.ne.i32       $91, $5, $90
  jt               $91, .L1550.13
.L1551.13:
  mov.i64          $92, 2
  add.i64          $4, $4, $92
  mov.i64          $1000, $4
  mov.i64          $1008, $2
  mov.i64          $93, camlCamlinternalFormat__parse_spaces_6740
  call.i64.caml   $1002, $93, $1000, $1008 @caml_frame
  mov.i64          $4, $1002
.L1550.13:
  cmp.ne.i32       $94, $4, $1
  jt               $94, .L1549.13
  mov.i64          $5, $4
  jmp              .L1545.13
.L1549.13:
  mov.i64          $95, 1
  sra.i64          $5, $4, $95
  mov.i64          $96, -8
  add.i64          $97, $0, $96
  ld.8.u64         $6, [$97]
  mov.i64          $98, 10
  srl.i64          $6, $6, $98
  mov.i64          $99, 3
  sll.i64          $6, $6, $99
  mov.i64          $100, 1
  sub.i64          $6, $6, $100
  add.i64          $7, $0, $6
  ld.1.u64         $7, [$7]
  sub.i64          $6, $6, $7
  cmp.le.i32       $101, $6, $5
  jt               $101, .L1560.13
  add.i64          $5, $0, $5
  ld.1.u64         $5, [$5]
  mov.i64          $102, 1
  sll.i64          $5, $5, $102
  mov.i64          $103, 1
  add.i64          $5, $5, $103
  mov.i64          $104, 97
  cmp.lt.i32       $105, $5, $104
  jt               $105, .L1548.13
  mov.i64          $106, 117
  cmp.lt.i32       $107, $5, $106
  jt               $107, .L1546.13
  jmp              .L1547.13
.L1548.13:
  mov.i64          $108, 91
  cmp.eq.i32       $109, $5, $108
  jt               $109, .L1546.13
.L1547.13:
  mov.i64          $5, $4
  jmp              .L1545.13
.L1546.13:
  mov.i64          $110, 56
  add.i64          $5, $2, $110
  mov.i64          $111, 2
  add.i64          $6, $4, $111
  mov.i64          $1000, $4
  mov.i64          $1008, $6
  mov.i64          $1016, $5
  mov.i64          $112, camlCamlinternalFormat__parse_int_6742
  call.i64.caml   $1002, $112, $1000, $1008, $1016 @caml_frame
  mov.i64          $5, $1002
.L1545.13:
  cmp.ne.i32       $113, $4, $5
  jt               $113, .L1544.13
  mov.i64          $4, 1
  jmp              .L1541.13
.L1544.13:
  mov.i64          $114, .L1561.13
  frame.i64        $115, 0
  st.8             [$115], $114
  jmp              .L1542.13
.L1561.13:
  mov.i64          $116, caml_exn_val
  ld.8.i64         $2000, [$116]
  mov.i64          $4, $2000
  mov.i64          $6, caml_exn_Failure
  ld.8.u64         $7, [$4]
  cmp.ne.i32       $117, $7, $6
  jt               $117, .L1543.13
  mov.i64          $4, camlCamlinternalFormat__const_block_6732
  mov.i64          $1000, $4
  mov.i64          $118, camlCamlinternalFormat__failwith_message_6667
  call.i64.caml   $1002, $118, $1000 @caml_frame
  mov.i64          $4, $1002
  ld.8.u64         $6, [$4]
  mov.i64          $1000, $0
  mov.i64          $1008, $4
  call.i64.caml   $1002, $6, $1000, $1008 @caml_frame
  mov.i64          $4, $1002
  jmp              .L1541.13
.L1543.13:
  mov.i64          $2000, $4
  mov.i64          $119, caml_raise_exception
  tcall.c          $119, $2000 @caml_raise_frame
.L1542.13:
  mov.i64          $120, caml_exception_pointer
  frame.i64        $121, 0
  frame.i64        $122, 8
  frame.i64        $123, 16
  ld.8.u64         $124, [$120]
  st.8             [$123], $124
  mov.i64          $125, $rsp
  st.8             [$122], $125
  st.8             [$120], $121
  sub.i64          $6, $5, $4
  mov.i64          $126, 1
  add.i64          $6, $6, $126
  mov.i64          $1000, $0
  mov.i64          $1008, $4
  mov.i64          $1016, $6
  mov.i64          $127, camlStdlib__bytes__sub_213
  invoke.i64.caml $1002, $127, $1000, $1008, $1016, .L1561.13 @caml_frame
  mov.i64          $4, $1002
  mov.i64          $1000, $4
  mov.i64          $128, caml_int_of_string
  mov.i64          $129, caml_c_call_i64
  invoke.i64.caml_ext $1002, $129, $128, $1000, .L1561.13 @caml_frame
  mov.i64          $4, $1002
  mov.i64          $130, caml_exception_pointer
  frame.i64        $132, 16
  ld.8.i64         $131, [$132]
  st.8             [$130], $131
.L1541.13:
  cmp.ne.i32       $133, $5, $1
  jt               $133, .L1540.13
  jmp              .L1538.13
.L1540.13:
  mov.i64          $134, 1
  sra.i64          $6, $5, $134
  mov.i64          $135, -8
  add.i64          $136, $0, $135
  ld.8.u64         $7, [$136]
  mov.i64          $137, 10
  srl.i64          $7, $7, $137
  mov.i64          $138, 3
  sll.i64          $7, $7, $138
  mov.i64          $139, 1
  sub.i64          $7, $7, $139
  add.i64          $8, $0, $7
  ld.1.u64         $8, [$8]
  sub.i64          $7, $7, $8
  cmp.le.i32       $140, $7, $6
  jt               $140, .L1560.13
  add.i64          $6, $0, $6
  ld.1.u64         $6, [$6]
  mov.i64          $141, 1
  sll.i64          $6, $6, $141
  mov.i64          $142, 1
  add.i64          $6, $6, $142
  mov.i64          $143, 19
  cmp.eq.i32       $144, $6, $143
  jt               $144, .L1539.13
  mov.i64          $145, 65
  cmp.ne.i32       $146, $6, $145
  jt               $146, .L1538.13
.L1539.13:
  mov.i64          $147, 2
  add.i64          $5, $5, $147
  mov.i64          $1000, $5
  mov.i64          $1008, $2
  mov.i64          $148, camlCamlinternalFormat__parse_spaces_6740
  call.i64.caml   $1002, $148, $1000, $1008 @caml_frame
  mov.i64          $5, $1002
.L1538.13:
  cmp.eq.i32       $149, $5, $1
  jt               $149, .L1537.13
  mov.i64          $1, camlCamlinternalFormat__const_block_6732
  mov.i64          $1000, $1
  mov.i64          $150, camlCamlinternalFormat__failwith_message_6667
  call.i64.caml   $1002, $150, $1000 @caml_frame
  mov.i64          $1, $1002
  ld.8.u64         $2, [$1]
  mov.i64          $1000, $0
  mov.i64          $1008, $1
  call.i64.caml   $1002, $2, $1000, $1008 @caml_frame
  mov.i64          $1, $1002
  jmp              .L1536.13
.L1537.13:
  mov.i64          $1, 1
.L1536.13:
  mov.i64          $151, -8
  add.i64          $152, $3, $151
  ld.8.u64         $1, [$152]
  mov.i64          $153, 10
  srl.i64          $1, $1, $153
  mov.i64          $154, 2
  cmp.ge.i32       $155, $1, $154
  jt               $155, .L1530.13
  ld.8.u64         $1, [$3]
  mov.i64          $2, 432345564227567720
  cmp.ge.i32       $156, $1, $2
  jt               $156, .L1533.13
  mov.i64          $2, 288230376159473512
  cmp.ne.i32       $157, $1, $2
  jt               $157, .L1535.13
  mov.i64          $0, 7
  jmp              .L1528.13
.L1535.13:
  mov.i64          $2, 360287970189669992
  cmp.ne.i32       $158, $1, $2
  jt               $158, .L1534.13
  mov.i64          $0, 5
  jmp              .L1528.13
.L1534.13:
  mov.i64          $2, 432345564227567714
  cmp.eq.i32       $159, $1, $2
  jt               $159, .L1529.13
  jmp              .L1530.13
.L1533.13:
  mov.i64          $2, 432345564227567720
  cmp.ne.i32       $160, $1, $2
  jt               $160, .L1532.13
  mov.i64          $0, 1
  jmp              .L1528.13
.L1532.13:
  mov.i64          $2, 432345564227567734
  cmp.ne.i32       $161, $1, $2
  jt               $161, .L1531.13
  mov.i64          $0, 3
  jmp              .L1528.13
.L1531.13:
  mov.i64          $2, 504403158265495552
  cmp.eq.i32       $162, $1, $2
  jt               $162, .L1529.13
.L1530.13:
  mov.i64          $1, camlCamlinternalFormat__const_block_6732
  mov.i64          $1000, $1
  mov.i64          $163, camlCamlinternalFormat__failwith_message_6667
  call.i64.caml   $1002, $163, $1000 @caml_frame
  mov.i64          $1, $1002
  ld.8.u64         $2, [$1]
  mov.i64          $1000, $0
  mov.i64          $1008, $1
  call.i64.caml   $1002, $2, $1000, $1008 @caml_frame
  mov.i64          $0, $1002
  jmp              .L1528.13
.L1529.13:
  mov.i64          $0, 9
.L1528.13:
  mov.i64          $164, caml_alloc2
  call.i64.c       $1, $164 @caml_frame
  mov.i64          $2, 2048
  mov.i64          $165, -8
  add.i64          $166, $1, $165
  st.8             [$166], $2
  st.8             [$1], $4
  mov.i64          $167, 8
  add.i64          $168, $1, $167
  st.8             [$168], $0
  mov.i64          $1002, $1
  ret.i64          $1002
.L1560.13:
  mov.i64          $169, caml_ml_array_bound_error
  tcall.c          $169
