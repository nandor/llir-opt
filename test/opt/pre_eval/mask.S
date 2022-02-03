# RUN: %opt - -pass=pre-eval -emit=llir -entry=test -static -o=-

  .section data
  .align 8
aligned:
  .quad 0
  .end

  .section .text
test:
  .call c
.Lentry:
  mov.i64     $0, aligned
  trunc.i8    $1, $0
  mov.i8      $2, 1
  cmp.eq.i8   $3, $1, $2
  jump_cond         $3, .Ltrue, .Lfalse
.Ltrue:
  mov.i64     $4, 1234
  ret.i64     $4
.Lfalse:
  mov.i64     $4, 4567
  ret.i64     $4
  .end
