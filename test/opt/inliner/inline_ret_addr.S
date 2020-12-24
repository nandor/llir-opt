# RUN: %opt - -passes=inliner,dead-func-elim -emit=llir

caller_fork_addr:
  .visibility global_default
  .args       i32
  mov.i64     $0, callee_fork_addr
  arg.i32     $1, 0
  call.c.i64  $2, $0, $1
  ret.i64     $2
  .end

callee_fork_addr:
  .visibility global_hidden
  .args       i32
  arg.i32     $1, 0
  jt          $1, .Ltrue_addr
  get.i64     $2, $ret_addr
  ret.i64     $2
.Ltrue_addr:
  get.i64     $2, $ret_addr
  ret.i64     $2
  .end



caller_fork_regular:
  .visibility global_default
  .args       i32
  mov.i64     $0, callee_fork_regular
  arg.i32     $1, 0
  call.c.i64  $2, $0, $1
  ret.i64     $2
  .end

callee_fork_regular:
  .visibility global_hidden
  .args       i32
  arg.i32     $1, 0
  jt          $1, .Ltrue_regular
  mov.i64     $2, 5
  ret.i64     $2
.Ltrue_regular:
  mov.i64     $2, 6
  ret.i64     $2
  .end



  .section .text
caller_regular_addr:
  .visibility global_default
  mov.i64     $0, callee_regular_addr
  call.c.i64  $1, $0
  ret.i64     $1
  .end

callee_regular_addr:
  .visibility global_hidden
  mov.i64     $0, ext
  call.c      $0
  get.i64     $2, $ret_addr
  ret.i64     $2
  .end
