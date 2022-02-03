# RUN: %opt - -triple x86_64


xsaveopt:
  .args i64
  mov.i64       $0, -1
  arg.i64       $1, 0
  x86_x_save_opt  $1, $0
  ret
  .end


xrstor:
  .args i64
  mov.i64       $0, -1
  arg.i64       $1, 0
  x86_x_restore $1, $0
  ret
  .end
