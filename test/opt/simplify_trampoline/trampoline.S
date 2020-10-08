# ARGS: -passes=simplify-trampoline -emit=llir



callee:
  .args           i64, i64
  .call           c
  .visibility     global_hidden
  mov.i64         $0, 0
  ret             $0
  .end


trampoline:
  .call           caml
  .args           i64, i64
  .visibility     global_hidden
  arg.i64         $0, 0
  arg.i64         $1, 1
  mov.i64         $2, callee
  call.c.i64      $3, $2, $0, $1
  ret             $3
  .end


caller:
  .visibility     global_default
  mov.i64         $0, 0
  mov.i64         $1, 2
  mov.i64         $2, trampoline
  call.i64.caml   $3, $2, $0, $1
  ret             $3
  .end
