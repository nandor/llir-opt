


pc_simple:
  .visibility extern

  mov.i64 $1, $pc
  ret     $1
  .end


pc_frame:
  .visibility extern

  mov.i64 $1, $pc @caml_frame
  ret     $1
  .end
