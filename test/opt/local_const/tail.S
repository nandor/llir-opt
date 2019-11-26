
  .code
caml_c_call:
  .visibility extern
  .call caml_ext
  .args 0, i64
.LBBentry1369:
  mov.i64 $0, $ret_addr
  mov.i64 $1, caml_last_return_address
  st.8  $1, $0
  mov.i64 $2, $frame_addr
  mov.i64 $3, caml_bottom_of_stack
  st.8  $3, $2
  arg.i64 $4, 0
  tcall.0.c $4


  .data data
caml_bottom_of_stack:
  .quad 0
caml_last_return_address:
  .quad 0
