# RUN: %opt - -triple x86_64


f:
  .call caml

  mov.i64                $0, q
  call.caml.i64.i64.i64  $1, $2, $3, $0 @caml_frame
  ret.i64                $2
