camlTranslprim__primitive_is_ccall_2970:
  .visibility          global_default
  .call                caml
  .args                i64

  arg.i64              $0, 0 @caml_value
  mov.i64              $15007, -30
  add.i64              $0, $0, $15007
  switch               $0, .L310$132, .L311$132, .L311$132, .L311$132
.L311$132:
  mov.i64              $0, 0x1 @caml_value
  ret                  $0
.L310$132:
  mov.i64              $0, 0x3 @caml_value
  ret                  $0

