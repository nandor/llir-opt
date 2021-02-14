# RUN: %opt - -pass=pre-eval -emit=llir -entry=test -static -o=-

  .section data
test0:
  .quad test1
  .end

test1:
  .quad test2
  .end

test2:
  .quad 0
  .end


  .section .text
test:

  mov.i64   $0, test0
  ld.i64    $1, $0
  ld.i64    $2, $1

  ret       $2
  .end
