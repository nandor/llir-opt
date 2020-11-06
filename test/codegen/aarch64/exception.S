# RUN: %opt - -triple=aarch64


  .section .text
  .globl caml_raise_exn
caml_raise_exn:
  .call       caml
  .args       i64, i64, i64

  # Store the young pointer.
  arg.i64     $0, 0
  mov.i64     $10, offset_young_ptr
  add.i64     $11, $0, $10
  arg.i64     $1, 1
  st          [$11], $1

  # Raise the exception here.
  arg.i64     $10, 2
  mov.i64     $11, offset_exception_value
  add.i64     $11, $11, $0
  st          [$11], $10

  # Load the frame.
  mov.i64     $14, offset_exception_pointer
  add.i64     $14, $14, $0
  mov.i64     $15, 8
  ld.i64      $16, [$14]
  add.i64     $17, $16, $15
  add.i64     $18, $17, $15

  ld.i64      $19, [$16]    # Address.
  ld.i64      $20, [$17]    # Stack pointer.
  ld.i64      $21, [$18]    # Next frame.

  # Jump.
  # CHECK: str
  # CHECK: mov
  # CHECK: br
  # CHECK: .Lfunc_end0
  st          [$14], $21
  raise.caml  $19, $20
