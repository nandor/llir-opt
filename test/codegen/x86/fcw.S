
load_store:
  .stack_object 0, 16, 16
  .noinline
  .call c
  frame.i64 $0, 0, 0
  # CHECK: fnstcw
  fnstcw $0
  # CHECK: fldcw
  fldcw $0
  ret
