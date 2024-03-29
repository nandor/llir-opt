# RUN: %opt - -triple x86_64


syscall:
  .visibility global_default
  .args       i64, i64, i64

  arg.i64     $0, 0
  arg.i64     $1, 1
  arg.i64     $2, 2

  mov.i64     $3, 0
  syscall.i64 $5, $3, $0, $1, $2
  ret         $5
  .end


_Exit:
  .visibility          global_hidden
  .args                i32
  .call                c
  arg.i32              $3, 0
  s_ext.i64             $6, $3
  mov.i64              $2, 231
  syscall.i64          $1, $2, $6
  mov.i64              $5, 60
.LBB0_1$149:
  syscall.i64          $4, $5, $6
  jump                 .LBB0_1$149
  .end
