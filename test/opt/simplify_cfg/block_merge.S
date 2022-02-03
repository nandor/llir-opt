# RUN: %opt - -pass=simplify-cfg -emit=llir

.section .text
main:
  .visibility     global_default
  .call   c
  .args   i32, i64
.START:
  mov.i32    $0, 0
  mov.i32    $1, 1
  cmp.i32.eq $2, $1, $0
  jump       .BLOCK1
.BLOCK1:
  ret         $0
  .end
