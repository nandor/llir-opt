# RUN: %opt - -pass=move-elim -pass=dead-code-elim -pass=simplify-cfg -emit=llir


  # CHECK: thread_cond_same_target
  # CHECK: select
  .section .text
thread_cond_same_target:
  .visibility global_default
  .args       i32

  mov.i64     $0, 100
  arg.i32     $1, 0
  mov.i64     $2, 200
  jt          $1, .L1
.Lt:
  jump        .L4
.L1:
  jump        .L2
.L2:
  jump        .L3
.L3:
  jump        .L4
.L4:
  phi.i64     $3, .Lt, $2, .L3, $0
  ret.i64     $3
  .end

  # CHECK: thread_cond_same_pred
  # CHECK: mov i64:$0, 0
  .section .text
thread_cond_same_pred:
  .visibility global_default
  .args       i32

  mov.i64     $0, 100
  arg.i32     $1, 0
  jt          $1, .Lf1
.Lt2:
  jump        .Lt1
.Lt1:
  jump        .L5
.Lf1:
  jump        .L5
.L5:
  jump        .L6
.L6:
  mov.i64     $3, 0
  ret.i64     $3
  .end
