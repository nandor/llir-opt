# RUN: %opt - -passes=move-elim,dead-code-elim,simplify-cfg -emit=llir


  # CHECK: thread_cond_same_target
  # CHECK: select.i64
  .section .text
thread_cond_same_target:
  .visibility global_default
  .args       i32

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
  .end

  # CHECK: thread_cond_same_pred
  # CHECK: mov.i64 $0, 0
  .section .text
thread_cond_same_pred:
  .visibility global_default
  .args       i32

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
  .end
