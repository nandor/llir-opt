

caller:
  .call           c
  .args           0, i32

  mov.i64         $0, callee
  arg.i32         $1, 0
  tinvoke.i32.c   $0, $1, .Lhandler
.Lhandler:
  mov.i32         $2, 100
  ret.i32         $2

callee:
  .call           c
  .args           0, i32
  trap
