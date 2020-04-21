
load_store:
  .stack_object 0, 16, 16
  .noinline
  .args 0
  .call c
  frame.i64 $0, 0, 0
  # CHECK: fnstcw
  fnstcw $0
  # CHECK: fldcw
  fldcw $0
  ret
