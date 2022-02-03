# RUN: %opt - -pass=inliner -emit=llir
  .section .text

callee_loop:
  .args       i64
.Lentry_loop:
  mov.i64     $0, foo_loop
  arg.i64     $1, 0
  call.c      $0, $1
  jump        .Lentry_loop
  .end

caller_loop:
  .visibility global_default

  mov.i64     $0, 0
  mov.i64     $1, callee_loop
  call.c      $1, $0
  trap
  .end

foo_loop:
  .args       i64
  .noinline

  trap
  .end

