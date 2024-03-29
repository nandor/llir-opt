# RUN: %opt - -triple x86_64
  .section .text
_va_call:
  .call         c

  mov.i64       $8, _va_callee
  mov.i64       $7, 5
  mov.i64       $6, 0
  s_ext.f64      $6, $6
  mov.i32       $5, 1
  mov.i64       $4, 0
  s_ext.f64      $4, $4
  mov.i64       $3, 1

  call.f64.1.c  $9, $8, $7, $6, $5, $4, $3
  s_ext.i64      $10, $9
  ret.i32       $10
  .end

_va_tcall:
  .call         c

  mov.i64       $8, _va_callee
  mov.i64       $7, 5
  mov.i64       $6, 0
  s_ext.f64      $6, $6
  mov.i32       $5, 1
  mov.i64       $4, 0
  s_ext.f64      $4, $4
  mov.i64       $3, 1

  tcall.1.c     $8, $7, $6, $5, $4, $3
  .end


_va_callee:
  .args         i64
  ret
  .end


_va_forward:
  .stack_object 0, 32, 8
  .args         i64, i64
  .vararg
  .call c

  frame.i64     $1, 0, 0
  va_start       $1
  mov.i64       $2, _va_forward_callee
  call.c        $2, $1
  ret
  .end

_va_forward_callee:
  .args         i64
  .call         c
  ret
  .end
