# RUN: %opt - -triple x86_64

load_store:
  .stack_object 0, 16, 16
  .noinline
  .call c
  frame.i64 $0, 0, 0
  # CHECK: fnstcw
  x86_fn_st_cw $0
  # CHECK: fldcw
  x86_f_ld_cw $0
  ret
  .end
