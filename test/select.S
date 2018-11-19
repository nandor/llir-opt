  .text
_select_i32:
  .args        1
  arg.i32      $0, 0
  imm.i32      $1, 200
  imm.i32      $2, 100
  select.i32   $3, $0, $1, $2
  ret.i32      $3

_select_i64:
  .args        1
  arg.i32      $0, 0
  imm.i64      $1, 200
  imm.i64      $2, 100
  select.i64   $3, $0, $1, $2
  ret.i64      $3
