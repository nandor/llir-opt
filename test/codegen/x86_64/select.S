# RUN: %opt - -triple x86_64
  .section .text
_select_i32:
  .args        i32
  arg.i32      $0, 0
  mov.i32      $1, 200
  mov.i32      $2, 100
  select.i32   $3, $0, $1, $2
  ret.i32      $3
  .end

_select_i64:
  .args        i32
  arg.i32      $0, 0
  mov.i64      $1, 200
  mov.i64      $2, 100
  select.i64   $3, $0, $1, $2
  ret.i64      $3
  .end
