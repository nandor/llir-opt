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
  load.i64    $1, $0
  load.i64    $2, $1

  ret       $2
  .end
