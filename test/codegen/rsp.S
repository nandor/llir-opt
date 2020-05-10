
  .text
longjmp:
  .noinline
  .args         0, i64, i64
  .call         c

  arg.i64       $0, 0
  arg.i64       $1, 1
  set           $rsp, $0
  tcall.c       $1
