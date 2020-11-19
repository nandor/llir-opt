# RUN: %opt - -triple powerpc64le


indirect_call:
  .args       i64, i64, i64
  arg.i64     $0, 0
  arg.i64     $1, 1
  arg.i64     $2, 2
  mov.i64     $4, 5
  call.c.i64  $5, $2, $4, $1, $0
  ret.i64     $5
  .end
