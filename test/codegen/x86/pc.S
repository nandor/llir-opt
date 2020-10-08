


pc_simple:
  .visibility global_default

  mov.i64 $1, $pc
  ret     $1
  .end


pc_frame:
  .visibility global_default

  mov.i64 $1, $pc @caml_frame
  ret     $1
  .end
