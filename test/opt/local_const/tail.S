
  .section .text
tail_call_indirect:
  .visibility extern
  .call caml
  .args i64
.LBBentry1369:
  arg.i64 $4, 0
  tcall.0.c $4
  .end
