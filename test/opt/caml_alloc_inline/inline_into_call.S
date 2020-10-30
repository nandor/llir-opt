# RUN: %opt - -passes=caml-alloc-inliner


test:
  .call caml
  .args                       i64, i64

  arg.i64                     $0, 0
  arg.i64                     $1, 1

  mov.i64                     $5, caml_alloc1
  call.caml_alloc.i64.i64     $10, $11, $5, $0, $1, .Lcont @caml_frame
.Lcont:

  mov.i64                     $20, 101
  st                          [$11], $20
  mov.i64                     $21, 8
  add.v64                     $22, $11, $21
  st                          [$22], $20

  ret                         $10, $11, $22
