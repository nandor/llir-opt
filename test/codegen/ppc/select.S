# RUN: %opt - -triple powerpc64le

test_select:
  .args i32, i64, i64
  arg.i32     $0, 0
  arg.i64     $1, 1
  arg.i64     $2, 2

  select.i64  $3, $0, $1, $2
  ret         $3
