# RUN: %opt - -pass=stack-object-elim -emit=llir

# CHECK: func
func:
  .visibility global_default
  # CHECK: .stack_object
  .stack_object 0, 8, 8
  .stack_object 1, 8, 8
  .stack_object 2, 8, 8
  frame.i64     $0, 2, 4
  mov.i64       $1, 1
  store         $0, $1
  # CHECK: ret
  ret
  .end
