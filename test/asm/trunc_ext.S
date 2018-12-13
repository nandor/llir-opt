
trunc_ext:
  .args            0, f64
  .call            c

  arg.f64          $1, 0
  trunc.f32        $31, $1
  fext.f64         $32, $31

  ret.f64          $32
