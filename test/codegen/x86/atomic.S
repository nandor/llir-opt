_xchg:
  .call            c
  mov.i64          $1, 0
  mov.i64          $2, var
  x86_xchg.i64     $3, $2, $1
  ret              $3
  .end

_cmp_xchg:
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
