camlTranslprim__primitive_is_ccall_2970:
  .visibility          global_default
  .call                caml
  .args                v64

  arg.v64              $0, 0
  mov.i64              $15007, -30
  add.i64              $0, $0, $15007
  switch               $0, .L310$132, .L311$132, .L311$132, .L311$132
.L311$132:
  mov.v64              $0, 0x1
  ret                  $0
.L310$132:
  mov.v64              $0, 0x3
  ret                  $0

