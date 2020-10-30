# RUN: %opt - -passes=higher-order


apply:
  .call c
  .args i64, i64

  arg.i64         $0, 0
  arg.i64         $1, 1
  call.c.i64.i64  $2, $3, $0, $1
  ret             $2, $3
  .end


test:
  .call c
  .args i64
  arg.i64         $0, 0
  mov.i64         $1, 1
  mov.i64         $2, 2
  add.i64         $3, $0, $1
  add.i64         $4, $0, $2
  ret             $3, $3
  .end


call:
  .call c
  .visibility     global_default

  mov.i64         $0, apply
  mov.i64         $1, test
  mov.i64         $2, 5

  call.c.i64.i64  $3, $4, $0, $1, $2

  ret             $3, $4
  .end
