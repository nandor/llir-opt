# RUN: %opt - -emit=llir

  .section .text
caml_request_major_slice:
  .visibility global_hidden
  .call c
.LBBentry563:
  mov.i32 $6975, 1
  mov.i64 $6976, caml_requested_major_slice
  store.4  $6976, $6975
  mov.i64 $6978, caml_something_to_do
  store.4  $6978, $6975
  ret
  .end


  .section .data
caml_something_to_do:
  .long 0
caml_requested_major_slice:
  .long 0
