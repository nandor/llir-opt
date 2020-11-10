# RUN: %opt - -triple=riscv64 -mcpu=sifive-u74


ret_i8:
  mov.i8 $0, 100
  ret    $0
  .end

ret_i16:
  mov.i16 $0, 100
  ret    $0
  .end

ret_i32:
  mov.i32 $0, 100
  ret    $0
  .end

ret_i64:
  mov.i64 $0, 100
  ret    $0
  .end

ret_f32:
  mov.f32 $0, 100
  ret    $0
  .end

ret_f64:
  mov.f64 $0, 100
  ret    $0
  .end
