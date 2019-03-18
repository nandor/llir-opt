  .code
caml_program:
  .call caml
  .args 0
.LBBentry849:
  mov.i64 $0, caml_globals_inited @caml_value
  ld.8.u64  $1, $0
  mov.i64 $2, 1
  add.i64 $3, $1, $2
  st.8  $0, $3
  mov.i64 $5, camlStdlib__Pmakeblock_2239 @caml_value
  mov.i64 $6, camlStdlib__const_string_423 @caml_value
  mov.i64 $7, caml_c_call_i64_i64$caml_register_named_value
  call.2.caml_ext  $7, $6, $5 @caml_frame
  mov.i64 $9, 1
  mov.i64 $10, caml_fresh_oo_id
  call.1.i64.c  $11, $10, $9 @caml_value
  mov.i64 $12, camlStdlib__Pccall_2740 @caml_value
  st.8  $12, $11
  mov.i64 $14, camlStdlib__Pccall_2740 @caml_value
  ld.8.u64  $15, $14 @caml_value
  mov.i64 $16, camlStdlib__Pmakeblock_2739 + 8
  st.8  $16, $15
  mov.i64 $18, 9218868437227405312
  mov.i64 $19, caml_int64_float_of_bits_unboxed
  call.1.f64.c  $20, $19, $18
  mov.i64 $21, caml_alloc1
  call.0.i64.caml_alloc  $22, $21 @caml_frame @caml_value
  mov.i64 $23, 1277
  mov.i64 $24, -8
  add.i64 $25, $22, $24
  st.8  $25, $23
  st.8  $22, $20
  mov.i64 $28, camlStdlib__Pccall_2736 @caml_value
  st.8  $28, $22
  mov.i64 $30, -4503599627370496
  mov.i64 $31, caml_int64_float_of_bits_unboxed
  call.1.f64.c  $32, $31, $30
  mov.i64 $33, caml_alloc1
  call.0.i64.caml_alloc  $34, $33 @caml_frame @caml_value
  mov.i64 $35, 1277
  mov.i64 $36, -8
  add.i64 $37, $34, $36
  st.8  $37, $35
  st.8  $34, $32
  mov.i64 $40, camlStdlib__Pccall_2735 @caml_value
  st.8  $40, $34
  mov.i64 $42, 9218868437227405313
  mov.i64 $43, caml_int64_float_of_bits_unboxed
  call.1.f64.c  $44, $43, $42
  mov.i64 $45, caml_alloc1
  call.0.i64.caml_alloc $46, $45 @caml_frame @caml_value
  mov.i64 $47, 1277
  mov.i64 $48, -8
  add.i64 $49, $46, $48
  st.8  $49, $47
  st.8  $46, $44
  mov.i64 $52, camlStdlib__Pccall_2734 @caml_value
  st.8  $52, $46
  mov.i64 $54, 9218868437227405311
  mov.i64 $55, caml_int64_float_of_bits_unboxed
  call.1.f64.c  $56, $55, $54
  mov.i64 $57, caml_alloc1
  call.0.i64.caml_alloc  $58, $57 @caml_frame @caml_value
  mov.i64 $59, 1277
  mov.i64 $60, -8
  add.i64 $61, $58, $60
  st.8  $61, $59
  st.8  $58, $56
  mov.i64 $64, camlStdlib__Pccall_2733 @caml_value
  st.8  $64, $58
  mov.i64 $66, 4503599627370496
  mov.i64 $67, caml_int64_float_of_bits_unboxed
  call.1.f64.c  $68, $67, $66
  mov.i64 $69, caml_alloc1
  call.0.i64.caml_alloc  $70, $69 @caml_frame @caml_value
  mov.i64 $71, 1277
  mov.i64 $72, -8
  add.i64 $73, $70, $72
  st.8  $73, $71
  st.8  $70, $68
  mov.i64 $76, camlStdlib__Pccall_2732 @caml_value
  st.8  $76, $70
  mov.i64 $78, 4372995238176751616
  mov.i64 $79, caml_int64_float_of_bits_unboxed
  call.1.f64.c  $80, $79, $78
  mov.i64 $81, caml_alloc1
  call.0.i64.caml_alloc  $82, $81 @caml_frame @caml_value
  mov.i64 $83, 1277
  mov.i64 $84, -8
  add.i64 $85, $82, $84
  st.8  $85, $83
  st.8  $82, $80
  mov.i64 $88, camlStdlib__Pccall_2731 @caml_value
  st.8  $88, $82
  mov.i64 $90, 1
  mov.i64 $91, caml_c_call_i64$caml_ml_open_descriptor_in
  call.1.i64.caml_ext  $92, $91, $90 @caml_frame @caml_value
  mov.i64 $93, camlStdlib__Pccall_2730 @caml_value
  st.8  $93, $92
  mov.i64 $95, 3
  mov.i64 $96, caml_c_call_i64$caml_ml_open_descriptor_out
  call.1.i64.caml_ext $97, $96, $95 @caml_frame @caml_value
  mov.i64 $98, camlStdlib__Pccall_2729 @caml_value
  st.8  $98, $97
  mov.i64 $100, 5
  mov.i64 $101, caml_c_call_i64$caml_ml_open_descriptor_out
  call.1.i64.caml_ext  $102, $101, $100 @caml_frame @caml_value
  mov.i64 $103, camlStdlib__Pccall_2728 @caml_value
  st.8  $103, $102
  mov.i64 $105, caml_alloc1
  call.0.i64.caml_alloc  $106, $105 @caml_frame @caml_value
  mov.i64 $107, 1024
  mov.i64 $108, -8
  add.i64 $109, $106, $108
  st.8  $109, $107
  mov.i64 $111, camlStdlib__flush_all_805_closure @caml_value
  st.8  $106, $111
  mov.i64 $113, camlStdlib__exit_function_2689 @caml_value
  st.8  $113, $106
  mov.i64 $115, camlStdlib__do_at_exit_1519_closure @caml_value
  mov.i64 $116, camlStdlib__const_string_1542 @caml_value
  mov.i64 $117, caml_c_call_i64_i64$caml_register_named_value
  call.2.caml_ext  $117, $116, $115 @caml_frame
  mov.i64 $119, camlStdlib__Pccall_2728 @caml_value
  ld.8.u64  $120, $119 @caml_value
  mov.i64 $121, camlStdlib__Pmakeblock_2533 + 216
  st.8  $121, $120
  mov.i64 $123, camlStdlib__Pccall_2729 @caml_value
  ld.8.u64  $124, $123 @caml_value
  mov.i64 $125, camlStdlib__Pmakeblock_2533 + 208
  st.8  $125, $124
  mov.i64 $127, camlStdlib__Pccall_2730 @caml_value
  ld.8.u64  $128, $127 @caml_value
  mov.i64 $129, camlStdlib__Pmakeblock_2533 + 200
  st.8  $129, $128
  mov.i64 $131, camlStdlib__Pccall_2731 @caml_value
  ld.8.u64  $132, $131 @caml_value
  mov.i64 $133, camlStdlib__Pmakeblock_2533 + 112
  st.8  $133, $132
  mov.i64 $135, camlStdlib__Pccall_2732 @caml_value
  ld.8.u64  $136, $135 @caml_value
  mov.i64 $137, camlStdlib__Pmakeblock_2533 + 104
  st.8  $137, $136
  mov.i64 $139, camlStdlib__Pccall_2733 @caml_value
  ld.8.u64  $140, $139 @caml_value
  mov.i64 $141, camlStdlib__Pmakeblock_2533 + 96
  st.8  $141, $140
  mov.i64 $143, camlStdlib__Pccall_2734 @caml_value
  ld.8.u64  $144, $143 @caml_value
  mov.i64 $145, camlStdlib__Pmakeblock_2533 + 88
  st.8  $145, $144
  mov.i64 $147, camlStdlib__Pccall_2735 @caml_value
  ld.8.u64  $148, $147 @caml_value
  mov.i64 $149, camlStdlib__Pmakeblock_2533 + 80
  st.8  $149, $148
  mov.i64 $151, camlStdlib__Pccall_2736 @caml_value
  ld.8.u64  $152, $151 @caml_value
  mov.i64 $153, camlStdlib__Pmakeblock_2533 + 72
  st.8  $153, $152
  mov.i64 $155, camlStdlib__Pccall_2728 @caml_value
  ld.8.u64  $156, $155 @caml_value
  mov.i64 $157, camlStdlib + 224
  st.8  $157, $156
  mov.i64 $159, camlStdlib__Pccall_2729 @caml_value
  ld.8.u64  $160, $159 @caml_value
  mov.i64 $161, camlStdlib + 216
  st.8  $161, $160
  mov.i64 $163, camlStdlib__Pccall_2730 @caml_value
  ld.8.u64  $164, $163 @caml_value
  mov.i64 $165, camlStdlib + 208
  st.8  $165, $164
  mov.i64 $167, camlStdlib__Pccall_2731 @caml_value
  ld.8.u64  $168, $167 @caml_value
  mov.i64 $169, camlStdlib + 120
  st.8  $169, $168
  mov.i64 $171, camlStdlib__Pccall_2732 @caml_value
  ld.8.u64  $172, $171 @caml_value
  mov.i64 $173, camlStdlib + 112
  st.8  $173, $172
  mov.i64 $175, camlStdlib__Pccall_2733 @caml_value
  ld.8.u64  $176, $175 @caml_value
  mov.i64 $177, camlStdlib + 104
  st.8  $177, $176
  mov.i64 $179, camlStdlib__Pccall_2734 @caml_value
  ld.8.u64  $180, $179 @caml_value
  mov.i64 $181, camlStdlib + 96
  st.8  $181, $180
  mov.i64 $183, camlStdlib__Pccall_2735 @caml_value
  ld.8.u64  $184, $183 @caml_value
  mov.i64 $185, camlStdlib + 88
  st.8  $185, $184
  mov.i64 $187, camlStdlib__Pccall_2736 @caml_value
  ld.8.u64  $188, $187 @caml_value
  mov.i64 $189, camlStdlib + 80
  st.8  $189, $188
  mov.i64 $191, caml_globals_inited @caml_value
  ld.8.u64  $192, $191
  mov.i64 $193, 1
  add.i64 $194, $192, $193
  st.8  $191, $194
  mov.i64 $196, 1 @caml_value
  mov.i64 $197, camlExc__test_throw_from_c_11
  call.1.caml  $197, $196 @caml_frame
  mov.i64 $199, 1 @caml_value
  mov.i64 $200, camlExc__test_throw_from_ml_48
  call.1.caml  $200, $199 @caml_frame
  mov.i64 $202, caml_globals_inited @caml_value
  ld.8.u64  $203, $202
  mov.i64 $204, 1
  add.i64 $205, $203, $204
  st.8  $202, $205
  mov.i64 $207, camlStd_exit__simplify_fv_27 @caml_value
  mov.i64 $208, camlStdlib__exit_function_2689 @caml_value
  ld.8.u64  $209, $208 @caml_value
  ld.8.u64  $210, $209 @caml_value
  st.8  $207, $210
  mov.i64 $212, camlStd_exit__simplify_fv_27 @caml_value
  ld.8.u64  $213, $212 @caml_value
  mov.i64 $214, 1 @caml_value
  ld.8.u64  $215, $213 @caml_value
  mov.i64 $216, $213 @caml_value
  call.2.caml  $215, $214, $216 @caml_frame
  mov.i64 $218, caml_globals_inited @caml_value
  ld.8.u64  $219, $218
  mov.i64 $220, 1
  add.i64 $221, $219, $220
  st.8  $218, $221
  mov.i64 $223, 1
  ret $223

.extern caml_alloc1
.extern caml_c_call_i64$caml_ml_open_descriptor_in
.extern caml_c_call_i64$caml_ml_open_descriptor_out
.extern caml_c_call_i64_i64$caml_register_named_value
.extern caml_fresh_oo_id
.extern caml_globals_inited
.extern caml_int64_float_of_bits_unboxed
.extern camlExc__test_throw_from_c_11
.extern camlExc__test_throw_from_ml_48
.extern camlStd_exit__simplify_fv_27
.extern camlStdlib
.extern camlStdlib__const_string_1542
.extern camlStdlib__const_string_423
.extern camlStdlib__do_at_exit_1519_closure
.extern camlStdlib__exit_function_2689
.extern camlStdlib__flush_all_805_closure
.extern camlStdlib__Pccall_2728
.extern camlStdlib__Pccall_2729
.extern camlStdlib__Pccall_2730
.extern camlStdlib__Pccall_2731
.extern camlStdlib__Pccall_2732
.extern camlStdlib__Pccall_2733
.extern camlStdlib__Pccall_2734
.extern camlStdlib__Pccall_2735
.extern camlStdlib__Pccall_2736
.extern camlStdlib__Pccall_2740
.extern camlStdlib__Pmakeblock_2239
.extern camlStdlib__Pmakeblock_2533
.extern camlStdlib__Pmakeblock_2739
