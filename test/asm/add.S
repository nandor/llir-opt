  .text
add_32:
  .args   0, i32, i32

  arg.i32 $0, 0
  arg.i32 $1, 1
  add.i32 $2, $0, $1
  ret.i32 $2

add_64:
  .args   0, i64, i64

  arg.i64 $0, 0
  arg.i64 $1, 1
  add.i64 $2, $0, $1
  ret.i64 $2
