# RUN: %opt - -triple x86_64

test:
  .call caml

  mov.i64                       $1, f
  invoke.i64.i64.caml           $2, $3, $1, .Lcont, .Lthrow
.Lcont:
  # CHECK: addq %r15, %r14
  add.i64                       $4, $2, $3
  ret                           $4
.Lthrow:
  # CHECK: addq %r15, %r14
  # CHECK: addq %rax, %r14
  landing_pad.i64.i64.i64.caml  $5, $6, $7
  add.i64                       $9, $5, $6
  add.i64                       $10, $9, $7
  ret                           $10
