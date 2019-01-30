_xchg:
  .call        c
  mov.i64      $1, 0
  mov.i64      $2, var
  xchg.i64     $3, $2, $1
  ret.i64      $3

  .data data
var:
  .quad 0
