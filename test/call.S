  .text

_caller:
  .args        0, 0
  .call        c

  mov.i64      $0, _ret_i32
  call.i32.c   $1, $0

  mov.i64      $3, _ret_i64
  call.i64.c   $4, $3


  mov.i64      $6, _ret_f32
  call.f32.c   $7, $6


  mov.i64      $9, _ret_f64
  call.f64.c   $10, $9

  mov.i64      $2, _arg_i32
  call.c       $2, $1
  mov.i64      $5, _arg_i64
  call.c       $5, $4
  mov.i64      $8, _arg_f32
  call.c       $8, $7
  mov.i64      $11, _arg_f64
  call.c       $11, $10

  ret

_arg_i32:
  .args       1, 0
  ret

_arg_i64:
  .args       1, 0
  ret

_arg_f32:
  .args       1, 0
  ret

_arg_f64:
  .args       1, 0
  ret

_ret_i32:
  mov.i32     $0, 123
  ret.i32     $0

_ret_i64:
  mov.i64     $0, 123
  ret.i64     $0

_ret_f32:
  mov.f32     $0, 0
  ret.f32     $0

_ret_f64:
  mov.f64     $0, 0
  ret.f64     $0



_caller_many_args:
  mov.i64     $0, _callee_many_args
  mov.i64     $1, 1
  mov.i64     $2, 2
  mov.i64     $3, 3
  mov.i64     $4, 4
  mov.i64     $5, 5
  mov.i64     $6, 6
  mov.i64     $7, 7
  mov.i64     $8, 8
  mov.i64     $9, 9
  call.i64.c  $11, $0, $1, $2, $3, $4, $5, $6, $7, $8, $9
  ret.i64     $11

_callee_many_args:
  .args 9, 0
  mov.i64   $0, 0
  ret.i64   $0
