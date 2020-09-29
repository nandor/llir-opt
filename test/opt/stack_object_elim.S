# ARGS: -passes=stack-object-elim -emit=llir

# CHECK: func
func:
  .visibility extern
  # CHECK: .stack_object
  .stack_object 0, 8, 8
  .stack_object 1, 8, 8
  .stack_object 2, 8, 8
  frame.i64     $0, 2, 4
  mov.i64       $1, 1
  st.8          $0, $1
  # CHECK: ret
  ret
  .end
