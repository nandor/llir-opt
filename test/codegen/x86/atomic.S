# RUN: %opt - -triple x86_64

_xchg:
  .call            c
  mov.i64          $1, 0
  mov.i64          $2, var
  x86_xchg.i64     $3, $2, $1
  ret              $3
  .end

_cmp_xchg_8:
  .call           c
  .args           i64, i8, i8
  arg.i64         $0, 0
  arg.i8          $1, 1
  arg.i8          $2, 2
  x86_cmpxchg.i8  $3, $0, $1, $2
  ret             $3
  .end

_cmp_xchg_16:
  .call           c
  .args           i64, i16, i16
  arg.i64         $0, 0
  arg.i16         $1, 1
  arg.i16         $2, 2
  x86_cmpxchg.i16 $3, $0, $1, $2
  ret             $3
  .end

_cmp_xchg_32:
  .call           c
  .args           i64, i32, i32
  arg.i64         $0, 0
  arg.i32         $1, 1
  arg.i32         $2, 2
  x86_cmpxchg.i32 $3, $0, $1, $2
  ret             $3
  .end

_cmp_xchg_64:
  .call           c
  .args           i64, i64, i64
  arg.i64         $0, 0
  arg.i64         $1, 1
  arg.i64         $2, 2
  x86_cmpxchg.i64 $3, $0, $1, $2
  ret             $3
  .end

  .section .data
var:
  .quad 0
