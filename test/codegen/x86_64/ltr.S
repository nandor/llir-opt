# RUN: %opt - -triple x86_64

test:
  mov.i16 $0, 100
  x86_ltr $0
  ret
  .end
