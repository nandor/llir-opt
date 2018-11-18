  .text
_addr_direct:
  addr.i64     $0, data
  ret.i64      $0

_addr_pos:
  addr.i64     $0, data+8
  ret.i64      $0

_addr_neg:
  addr.i64     $0, data-8
  ret.i64      $0

_addr_block:
  addr.i64     $0, .LBB0
.LBB0:
  ret.i64      $0

  .data
  .quad 0
data:
  .quad 0
  .quad 0
