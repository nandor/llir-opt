# ARGS:

  .section .text
f:
  .call caml
  .args i64

  arg.i64          $2, 0 @caml_value
  mov.i64          $0, g
  call.i64.caml    $1, $0 @caml_frame((2 3) (() ()))
  ret              $2
