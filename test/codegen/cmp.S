  .text
compare_32:
  .args i64, i64
  arg.i64 $0, 0
  arg.i64 $1, 1
  cmp.i32.eq $2, $1, $0
  ret.i32 $2

compare_64:
  .args i64, i64
  arg.i64 $0, 0
  arg.i64 $1, 1
  cmp.i64.eq $2, $1, $0
  ret.i64 $2
