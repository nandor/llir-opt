# RUN: %opt - -passes=unused-arg -emit=llir


  .section .text
callee:
  .visibility global_default
  .call       c
  # .args i64, i64
  .args       i64, i32, i64

  arg.i64 $0, 0
  arg.i64 $1, 2
  add.i64 $2, $0, $1
  ret     $2
  .end

caller:
  .visibility global_default
  .call       c

  mov.i64     $0, 100
  mov.i32     $1, 200
  mov.i64     $2, 300

  mov.i64     $3, callee
  # CHECK: tail_call   $3, $0, $2
  tcall.c.i64 $3, $0, $1, $2
  .end
