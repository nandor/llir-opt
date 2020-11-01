# RUN: %opt - -triple aarch64

dmb:
  aarch64_dmb
  ret
