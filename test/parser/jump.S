# RUN: %opt - -emit=llir

.section .text
main:
  .visibility     global_default
  .call   c
  .args   i32, i64
.START:
  jump       .BLOCK1
.BLOCK1:
  ret
  .end
