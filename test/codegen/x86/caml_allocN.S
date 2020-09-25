# ARGS:

caml_alloc1_callee:
  .call               caml
  mov.i64             $2, 128
  mov.i64             $0, caml_allocN
  call.i64.caml_alloc $1, $0, $2
  ret                 $1

