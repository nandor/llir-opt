# RUN: %opt - -pass=inliner -emit=llir



caller:
  .call           caml
  .args           i64
  .visibility     global_default

  arg.i64         $0, 0
  mov.i64         $1, callee
  # CHECK: $sp
  # CHECK: $sp
  invoke.i64.caml $2, $1, $0, .Lcont, .Lland
.Lcont:
  ret             $2
.Lland:
  landing_pad.i64 $3
  mov.i64         $4, 10
  add.i64         $5, $3, $4
  ret             $5
  .end


callee:
  .call       caml
  .args       i64
  .visibility local

  arg.i64     $0, 0
  mov.i64     $1, 100

  mov.i64     $2, addr
  load.i64      $3, $2

  mov.i64     $4, stack
  load.i64      $5, $4

  cmp.le.i8   $6, $0, $1
  jump_cond         $6, .Ltrue, .Lfalse
.Ltrue:
  mov.i64     $7, 100
  add.i64     $8, $0, $7
  raise       $3, $5, $8
.Lfalse:
  mov.i64     $7, 200
  sub.i64     $8, $0, $7
  raise       $3, $5, $8
  .end
