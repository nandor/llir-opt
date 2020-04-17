# ARGS: -passes=inliner,dead-func-elim

caller_fork_addr:
  .visibility extern
  .args       0, i32
  mov.i64     $0, callee_fork_addr
  arg.i32     $1, 0
  call.c.i64  $2, $0, $1
  ret.i64     $2

callee_fork_addr:
  .visibility hidden
  .args       0, i32
  arg.i32     $1, 0
  jt          $1, .Ltrue_addr
  mov.i64     $2, $ret_addr
  ret.i64     $2
.Ltrue_addr:
  mov.i64     $2, $ret_addr
  ret.i64     $2



caller_fork_regular:
  .visibility extern
  .args       0, i32
  mov.i64     $0, callee_fork_regular
  arg.i32     $1, 0
  call.c.i64  $2, $0, $1
  ret.i64     $2

callee_fork_regular:
  .visibility hidden
  .args       0, i32
  arg.i32     $1, 0
  jt          $1, .Ltrue_regular
  mov.i64     $2, 5
  ret.i64     $2
.Ltrue_regular:
  mov.i64     $2, 6
  ret.i64     $2



  .code
caller_regular_addr:
  .visibility extern
  mov.i64     $0, callee_regular_addr
  call.c.i64  $1, $0
  ret.i64     $1

callee_regular_addr:
  .visibility hidden
  mov.i64     $0, ext
  call.c      $0
  mov.i64     $2, $ret_addr
  ret.i64     $2
