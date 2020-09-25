# ARGS:

caml_alloc1_callee:
  .call               caml
  mov.i64             $0, caml_alloc1
  call.i64.caml_alloc $1, $0
  ret                 $1

