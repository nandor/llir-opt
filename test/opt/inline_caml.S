  .code
caml_caller:
  .visibility         extern
  .call               caml

  mov.i64             $0, caml_c_call_i64_i64
  mov.i64             $1, 0
  mov.i64             $2, 1

  call.i64.caml_ext   $3, $0, $1, $2          @caml_call_frame
  ret                 $3

caml_c_call_i64_i64:
  .visibility         hidden
  .call               caml_ext
  .args               0, i64, i64

  mov.i64             $3, $ret_addr
  mov.i64             $4, caml_last_return_address
  st.8                [$4], $3

  mov.i64             $7, $frame_addr
  mov.i64             $6, caml_bottom_of_stack
  st.8                [$6], $7

  mov.i64             $8, c_callee
  arg.i64             $9, 0
  arg.i64             $10, 1
  tcall.i64.c         $8, $9, $10


c_callee:
  .visibility         hidden
  .call c
  .args               0, i64, i64

  arg.i64             $0, 0
  arg.i64             $1, 1
  add.i64             $2, $0, $1
  ret.i64             $2




  .data data
caml_last_return_address:
  .quad 0

caml_bottom_of_stack:
  .quad 0
