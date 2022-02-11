# RUN: %opt - -triple x86_64

test:
  .visibility global_default
  .args       i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64, i64

  arg.i64       $0, 0
  arg.i64       $1, 1
  arg.i64       $2, 2
  arg.i64       $3, 3

  arg.i64       $10, 10

  frame_call.c  $0, $1, $10

  add.i64       $10, $2, $3
  ret           $10
  .end
