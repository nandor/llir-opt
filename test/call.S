  .text
  .globl       _caller
_caller:
  .args        0
  .call        c

  addr.i64     $0, _callee_0
  call         c, $0

  addr.i64     $1, _callee_1
  call         c, $1

  ret

  .globl       _callee_0
_callee_0:
  .args       0
  .call       c
  ret

  .globl       _callee_1
_callee_1:
  .args       0
  .call       c
  ret
