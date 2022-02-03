# RUN: %opt - -triple x86_64

phi_exn:
.Lentry:
  mov.i64           $10, caml_program
  invoke.i64.caml   $11, $10, .Lhandler

.Lreturn:
  phi.i64           $12, .Lentry, $11, .Lhandler, $23
  ret.i64           $12

.Lhandler:
  mov.i64           $23, 5
  jump              .Lreturn

