# ARGS:

  .section .text
c_caller:
  .call c
  .args i64, i64, i64, i64

  mov.i64          $4, f
  call.i64.caml    $5, $4 @caml_frame()
  ret              $5

#  .section .text
#caml_caller:
#  .call caml
#  .args i64, i64, i64, i64
#
#  arg.i64          $0, 0 @caml_value
#  arg.i64          $1, 1 @caml_value
#  arg.i64          $2, 2 @caml_value
#  arg.i64          $3, 3 @caml_value
#  mov.i64          $4, g
#  call.i64.caml    $5, $4, $0, $1, $2, $3 @caml_frame()
#  ld.i64           $10, $0
#  ld.i64           $11, $1
#  ld.i64           $12, $2
#  ld.i64           $13, $3
#  add.i64          $14, $10, $11
#  add.i64          $15, $12, $13
#  add.i64          $16, $14, $15
#  ret              $16
