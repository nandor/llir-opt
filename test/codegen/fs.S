__init_ssp:
  .visibility          extern
  .args                i64
  .call                c

  mov.i64              $13, 0
  mov.i64              $11, 40
  mov.i64              $10, $fs
  add.i64              $12, $10, $11
  st                   [$12], $13
  ret
