# RUN: %opt - -triple=aarch64


test:
  .args i128

  arg.i128  $0, 0
  mov.i128  $1, 1
  add.i128  $2, $0, $1
  ret       $2
