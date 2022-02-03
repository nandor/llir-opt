# RUN: %opt - -triple=aarch64


test:
  .args i128, i128

  arg.i128  $0, 0
  arg.i128  $1, 1
  mov.i128  $2, 1
  add.i128  $3, $0, $2
  jump      .Lcont
.Lcont:
  add.i128  $4, $3, $1
  ret       $4
