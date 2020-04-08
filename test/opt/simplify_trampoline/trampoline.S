# ARGS: -passes=simplify-trampoline -S



callee:
  .args           0, i64, i64
  .call           c
  .visibility     hidden
  mov.i64         $0, 0
  ret             $0


trampoline:
  .call           caml
  .args           0, i64, i64
  .visibility     hidden
  arg.i64         $0, 0
  arg.i64         $1, 1
  mov.i64         $2, callee
  call.c.i64      $3, $2, $0, $1
  ret             $3


caller:
  .visibility     extern
  mov.i64         $0, 0
  mov.i64         $1, 2
  mov.i64         $2, trampoline
  call.i64.caml   $3, $2, $0, $1
  ret             $3
