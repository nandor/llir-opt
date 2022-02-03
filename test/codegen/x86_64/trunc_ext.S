# RUN: %opt - -triple x86_64

trunc_ext:
  .args            f64
  .call            c

  arg.f64          $1, 0
  trunc.f32        $31, $1
  f_ext.f64         $32, $31

  ret              $32
  .end
