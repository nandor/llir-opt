# RUN: %opt - -pass=specialise -emit=llir
  .section .text
entry_call:
  .call c
  .visibility global_default

  mov.i64     $0, trampoline1
  mov.i64     $1, callee1
  mov.i64     $2, 100
  call.c.i64  $3, $0, $1, $2

  mov.i64     $0, trampoline2
  mov.i64     $1, callee2
  mov.i64     $2, 102
  call.c.i64  $3, $0, $1, $2

  ret
  .end

# CHECK trampoline1$specialised$callee1

trampoline1:
  .call c
  .args   i64, i64
  .visibility local

  arg.i64 $0, 0
  arg.i64 $1, 1
  mov.i64 $2, 2
  tcall.i64.c $0, $1, $2
  .end

callee1:
  .args   i64, i64
  .call c
  .visibility local

  arg.i64 $0, 0
  arg.i64 $1, 1
  add.i64 $2, $0, $1
  ret    $2
  .end

# CHECK trampoline2$specialised$callee2

trampoline2:
  .call c
  .args   i64, i64
  .visibility local

  arg.i64 $0, 0
  arg.i64 $1, 1
  mov.i64 $2, 2
  tcall.i64.c $0, $1, $2
  .end

callee2:
  .args   i64, i64
  .call c
  .visibility local

  arg.i64 $0, 0
  arg.i64 $1, 1
  sub.i64 $2, $0, $1
  ret     $2
  .end


