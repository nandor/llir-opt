# RUN: %opt - -triple aarch64

test:
  .call caml

  mov.i64                       $1, f
  invoke.i64.i64.caml           $2, $3, $1, .Lcont, .Lthrow
.Lcont:
  # CHECK: add

  add.i64                       $4, $2, $3
  ret                           $4
.Lthrow:
  # CHECK: add
  # CHECK: add
  landing_pad.i64.i64.i64.caml  $5, $6, $7
  add.i64                       $9, $5, $6
  add.i64                       $10, $9, $7
  ret                           $10
