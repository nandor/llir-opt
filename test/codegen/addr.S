  .code
_addr_direct:
  mov.i64      $0, data
  ret.i64      $0

_addr_pos:
  mov.i64      $0, data+8
  ret.i64      $0

_addr_neg:
  mov.i64      $0, data-8
  ret.i64      $0

_addr_block:
  mov.i64      $0, .LBB0
.LBB0:
  ret.i64      $0

  .data data
  .quad 0
data:
  .quad 0
  .quad 0
