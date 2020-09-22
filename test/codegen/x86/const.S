

const_i128:
  .visibility extern

  mov.i64   $0, -1
  sext.i64  $1, $0
  ret.i64   $1
