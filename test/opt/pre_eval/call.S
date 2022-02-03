# RUN: %opt - -pass=pre-eval -emit=llir

  .section .text
main:
.Lentry:
  mov.i64   $0, func_b
  call.c    $0
  jump      .Lfunc_c
.Lfunc_c:
  mov.i64   $1, func_c
  call.c    $1
  jump      .Lfinal
.Lfinal:
  mov.i64   $0, a
  mov.i64   $1, 3
  store.i64    [$0], $1
  ret
  .end

func_b:
  .call     c
.Lentry_b:
  mov.i64   $0, b
  mov.i64   $1, 1
  store.i64    [$0], $1
  ret
  .end

func_c:
  .call     c
.Lentry_c:
  mov.i64   $0, c
  mov.i64   $1, 2
  store.i64    [$0], $1
  ret
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
