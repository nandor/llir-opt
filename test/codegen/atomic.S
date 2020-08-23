_xchg:
  .call        c
  mov.i64      $1, 0
  mov.i64      $2, var
  xchg.i64     $3, $2, $1
  ret          $3

_cmp_xchg:
  .call       c
  .args       i64, i64, i64
  arg.i64     $0, 0
  arg.i64     $1, 1
  arg.i64     $2, 2
  cmpxchg.i64 $3, $0, $1, $2
  ret         $3

  .section .data
var:
  .quad 0
