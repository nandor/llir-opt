# RUN: %opt - -triple x86_64

const_i128:
  .visibility global_default

  mov.i64   $0, -1
  s_ext.i64  $1, $0
  ret.i64   $1
