  .text

sign_extend:
  .call            c
  .args            0

  mov.i64          $1, 1
  undef.i32        $66
  jt               $66, .Lother
  sext.i128        $95, $1
  mov.i64          $5, 1
.Lother:
  trap


hmul:
  .args            0, i64
  arg.i64          $1, 0
  mov.i64          $0, 2582544170319337227
  sext.i128        $2, $1
  sext.i128        $3, $0
  mul.i128         $4, $2, $3
  mov.i8           $5, 64
  srl.i128         $4, $4, $5
  trunc.i64        $0, $4
  ret.i64          $0


lmul:
  .args            0, i64

  arg.i64          $1, 0
  mov.i64          $0, 2582544170319337227
  mul.i64          $4, $1, $0
  ret.i64          $4
