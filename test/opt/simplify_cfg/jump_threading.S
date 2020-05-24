# ARGS: -passes=move-elim,dead-code-elim,simplify-cfg


  # CHECK: thread_cond_same_target
  # CHECK: cmov
  .text
thread_cond_same_target:
  .visibility extern
  .args       0, i32

  mov.i64     $0, 100
  arg.i32     $1, 0
  mov.i64     $2, 200
  jt          $1, .L1
.Lt:
  jmp         .L4
.L1:
  jmp         .L2
.L2:
  jmp         .L3
.L3:
  jmp         .L4
.L4:
  phi.i64     $3, .Lt, $2, .L3, $0
  ret.i64     $3

  # CHECK: thread_cond_same_pred
  # CHECK: xorl
  .text
thread_cond_same_pred:
  .visibility extern
  .args       0, i32

  mov.i64     $0, 100
  arg.i32     $1, 0
  jt          $1, .Lf1
.Lt2:
  jmp         .Lt1
.Lt1:
  jmp         .L5
.Lf1:
  jmp         .L5
.L5:
  jmp         .L6
.L6:
  mov.i64     $3, 0
  ret.i64     $3
