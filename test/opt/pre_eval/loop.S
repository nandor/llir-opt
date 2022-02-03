# RUN: %opt - -pass=pre-eval -emit=llir

  .section .text
main:
  .args     i64
.Lentry:
  mov.i64   $0, 0
  mov.i64   $200, c
  store.i64    $200, $0
  jump      .Lloop
.Lloop:
  mov.i64   $200, b
  store.i64    $200, $0
  mov.i64   $2, 1
  add.i64   $0, $0, $2
  mov.i64   $100, 100
  cmp.i8.lt $4, $0, $100
  jump_cond $4, .Lloop, .Lend
.Lend:
  mov.i64   $300, a
  load.i64    $301, [$300]
  mov.i64   $302, 1
  add.i64   $303, $301, $302
  store.i64    [$300], $303
  ret.i64   $303
  .end


  .section .data
a:
  .quad 0
  .end

b:
  .quad 0
  .end

c:
  .quad 0
  .end
