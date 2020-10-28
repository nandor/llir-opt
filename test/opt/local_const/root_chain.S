
  .section .text
live_caml_roots:
  .visibility global_default
  .stack_object          0, 16, 16
  .stack_object          1, 16, 16
  .stack_object          2, 16, 16


  mov.i64                $0, caml_local_roots

  mov.i64                $20, caml_alloc1
  call.i64.caml_alloc    $21, $20 @caml_frame
  frame.i64              $22, 0, 8
  st                     [$22], $21
  frame.i64              $23, 0, 0
  ld.i64                 $24, [$0]
  st                     [$23], $24
  st                     [$0], $24

  mov.i64                $30, caml_alloc1
  call.i64.caml_alloc    $31, $30 @caml_frame
  frame.i64              $32, 1, 8
  st                     [$32], $31
  frame.i64              $33, 1, 0
  ld.i64                 $34, [$0]
  st                     [$33], [$34]
  st                     [$0], $34

  mov.i64                $40, caml_alloc1
  call.i64.caml_alloc    $41, $40 @caml_frame
  frame.i64              $42, 2, 8
  st                     [$42], $41
  frame.i64              $43, 2, 0
  ld.i64                 $44, [$0]
  st                     [$43], [$44]
  st                     [$0], $44

  ret
  .end

  .section .data
caml_local_roots:
  .quad 0
