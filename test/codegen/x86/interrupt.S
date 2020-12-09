# RUN: %opt - -triple x86_64


test:
  x86_sti
  x86_cli
  x86_hlt
  x86_pause
  ret
