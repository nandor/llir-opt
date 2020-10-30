# RUN: %opt - -passes=pre-eval -emit=llir

  .section .text
main:
.Lentry:
  mov.i64   $0, func_b
  call.c    $0
  jmp.i64   .Lfunc_c
.Lfunc_c:
  mov.i64   $1, func_c
  call.c    $1
  jmp.i64   .Lfinal
.Lfinal:
  mov.i64   $0, a
  mov.i64   $1, 3
  st.i64    [$0], $1
  ret
  .end

func_b:
  .call     c
.Lentry_b:
  mov.i64   $0, b
  mov.i64   $1, 1
  st.i64    [$0], $1
  ret
  .end

func_c:
  .call     c
.Lentry_c:
  mov.i64   $0, c
  mov.i64   $1, 2
  st.i64    [$0], $1
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
