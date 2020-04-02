  .extern caml_code_begin
  .extern caml_code_end

  .code
ref_code:
  .args    0
  mov.i64  $0, caml_code_begin
  ret.i64  $0
