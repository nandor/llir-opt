# RUN: %opt - -triple x86_64

__init_ssp:
  .visibility          global_default
  .args                i64
  .call                c

  mov.i64              $13, 0
  mov.i64              $11, 40
  # CHECK:             %fs:0
  mov.i64              $10, $fs
  add.i64              $12, $10, $11
  st                   [$12], $13
  ret
