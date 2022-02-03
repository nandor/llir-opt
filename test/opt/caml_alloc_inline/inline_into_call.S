# RUN: %opt - -pass=caml-alloc-inliner -emit=llir


test_x86_64:
  .call caml
  .args                       i64, i64

  arg.i64                     $0, 0
  arg.i64                     $1, 1

  mov.i64                     $5, caml_alloc1
  call.caml_alloc.i64.i64     $10, $11, $5, $0, $1, .Lcont1 @caml_frame
.Lcont1:

  mov.i64                     $20, 101
  store                          [$11], $20
  mov.i64                     $21, 8
  add.v64                     $22, $11, $21
  store                          [$22], $20

  ret                         $10, $11, $22
  .end



test_aarch64:
  .call caml
  .args                       i64, i64, i64

  arg.i64                     $0, 0
  arg.i64                     $1, 1
  arg.i64                     $2, 2

  mov.i64                     $5, caml_alloc1
  call.caml_alloc.i64.i64.i64 $10, $11, $12, $5, $0, $1, $2, .Lcont2 @caml_frame
.Lcont2:

  mov.i64                     $20, 101
  store                          [$11], $20
  mov.i64                     $21, 8
  add.v64                     $22, $11, $21
  store                          [$22], $20

  ret                         $10, $11, $12, $22
