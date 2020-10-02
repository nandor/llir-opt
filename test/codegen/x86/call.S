  .section .text
_caller:
  .call        c

  mov.i64      $0, _ret_i32
  call.i32.c   $1, $0

  mov.i64      $2, _ret_i64
  call.i64.c   $3, $2

  mov.i64      $4, _ret_f32
  call.f32.c   $5, $4

  mov.i64      $6, _ret_f64
  call.f64.c   $7, $6

  mov.i64      $8, _arg_i32
  call.c       $8, $1

  mov.i64      $9, _arg_i64
  call.c       $9, $3

  mov.i64      $10, _arg_f32
  call.c       $10, $5

  mov.i64      $11, _arg_f64
  call.c       $11, $7

  ret
  .end

_arg_i32:
  .args       i32
  ret
  .end

_arg_i64:
  .args       i64
  ret
  .end

_arg_f32:
  .args       f32
  ret
  .end

_arg_f64:
  .args       f64
  ret
  .end

_ret_i32:
  mov.i32     $0, 123
  ret.i32     $0
  .end

_ret_i64:
  mov.i64     $0, 123
  ret.i64     $0
  .end

_ret_f32:
  mov.f32     $0, 0
  ret.f32     $0
  .end

_ret_f64:
  mov.f64     $0, 0
  ret.f64     $0
  .end



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
  .end

_callee_many_args:
  .args     i64, i64, i64, i64, i64, i64, i64, i64, i64
  mov.i64   $0, 0
  ret.i64   $0
  .end
