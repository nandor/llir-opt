

const_i128:
  .visibility global_default

  mov.i64   $0, -1
  sext.i64  $1, $0
  ret.i64   $1
