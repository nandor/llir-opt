camlStdlib__array__append_179:
  .visibility extern
  .call caml
  .args 0, i64, i64

  arg.i64 $0, 0 @caml_value
  arg.i64 $1, 1 @caml_value
  mov.i64 $2, -8
  add.i64 $3, $0, $2
  ld.8.u64  $4, $3
  mov.i64 $5, 9
  srl.i64 $6, $4, $5
  mov.i64 $7, 1
  or.i64  $8, $6, $7
  mov.i64 $9, 1
  cmp.ne.i32 $10, $8, $9
  jcc $10, .L127$115, .LBBterm8868
.LBBterm8868:
  mov.i64 $12, camlStdlib__array__copy_163
  tcall.1.caml  $12, $1 @caml_frame
.L127$115:
  mov.i64 $14, -8
  add.i64 $15, $1, $14
  ld.8.u64  $16, $15
  mov.i64 $17, 9
  srl.i64 $18, $16, $17
  mov.i64 $19, 1
  or.i64  $20, $18, $19
  mov.i64 $21, 1
  cmp.ne.i32 $22, $20, $21
  jcc $22, .L126$115, .LBBterm8869
.LBBterm8869:
  mov.i64 $24, 1
  mov.i64 $27, caml_c_call_i64_i64_i64$caml_array_sub
  call.3.caml_ext.i64 $28, $27, $0, $24, $8 @caml_frame @caml_value
  ret $28
.L126$115:
  mov.i64 $32, caml_c_call_i64_i64$caml_array_append
  call.2.caml_ext.i64 $33, $32, $0, $1 @caml_frame @caml_value
  ret $33

caml_c_call_i64_i64$caml_array_append:
  .visibility hidden
  .call caml_ext
  .stack_object 1, 16, 16
  .stack_object 2, 16, 16
  .stack_object 3, 16, 16
  .args 0, i64, i64

  mov.i64 $0, $ret_addr
  mov.i64 $1, caml_last_return_address
  st.8  $1, $0
  mov.i64 $3, $frame_addr
  mov.i64 $4, caml_bottom_of_stack
  st.8  $4, $3
  mov.i64 $6, caml_array_append
  arg.i64 $7, 0
  arg.i64 $8, 1
  mov.i64 $9, 8
  frame.i64 $10, 1, 0
  or.i64  $11, $10, $9
  st.8  $11, $8
  frame.i64 $13, 1, 0
  st.8  $13, $7
  frame.i64 $15, 2, 0
  or.i64  $16, $15, $9
  mov.i64 $17, 0
  st.8  $16, $17
  frame.i64 $19, 2, 0
  st.8  $19, $17
  mov.i64 $21, -8
  add.i64 $22, $7, $21
  ld.8.i64  $23, $22
  mov.i64 $24, 10
  srl.i64 $25, $23, $24
  frame.i64 $26, 3, 0
  st.8  $26, $25
  add.i64 $28, $8, $21
  ld.8.i64  $29, $28
  srl.i64 $30, $29, $24
  frame.i64 $31, 3, 0
  or.i64  $32, $31, $9
  st.8  $32, $30
  mov.i64 $34, 2
  mov.i64 $35, caml_array_gather$173
  frame.i64 $36, 1, 0
  frame.i64 $37, 2, 0
  frame.i64 $38, 3, 0
  call.4.fast.i64 $39, $35, $34, $36, $37, $38
  ret $39

.extern camlStdlib__array__copy_163
.extern caml_array_sub
.extern caml_c_call_i64_i64_i64
.extern caml_c_call_i64_i64_i64$caml_array_sub
.extern caml_array_append
.extern caml_c_call_i64_i64
.extern caml_last_return_address
.extern caml_bottom_of_stack
.extern caml_array_gather$173
