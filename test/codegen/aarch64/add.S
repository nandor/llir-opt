# ARGS: -triple=aarch64
  .section .text
add_32:
  .args   i32, i32

  arg.i32 $0, 0
  arg.i32 $1, 1
  # CHECK: add
  add.i32 $2, $0, $1
  # CHECK: ret
  ret.i32 $2
  .end

add_64:
  .args   i64, i64

  arg.i64 $0, 0
  arg.i64 $1, 1
  # CHECK: add
  add.i64 $2, $0, $1
  # CHECK: ret
  ret.i64 $2
  .end
