# RUN: %opt - -triple x86_64

  .section .text
test:
  .visibility          global_default
  .args                i32
  .call                c
  arg.i32              $1, 0
  s_ext.i64            $2, $1
  mov.i64              $3, 15
  add.i64              $4, $2, $3
  mov.i64              $5, -16
  and.i64              $6, $4, $5
  alloca.i64           $7, $6, 0
  mov.i32              $8, 5
  store.4              [$7], $8
  mov.i64              $9, sink
  call.c               $9, $7
  mov.i32              $10, 0
  ret                  $10
