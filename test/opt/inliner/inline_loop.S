  .text

callee_loop:
  .args       0, i64
.Lentry_loop:
  mov.i64     $0, foo_loop
  arg.i64     $1, 0
  call.c      $0, $1
  jmp         .Lentry_loop

caller_loop:
  .visibility extern
  .args       0

  mov.i64     $0, 0
  mov.i64     $1, callee_loop
  call.c      $1, $0
  trap

foo_loop:
  .args       0, i64
  .noinline

  trap

