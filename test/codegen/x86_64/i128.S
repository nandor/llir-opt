# RUN: %opt - -triple x86_64
  .section .text

sign_extend:
  .call            c

  mov.i64          $1, 1
  undef.i32        $66
  jt               $66, .Lother
  s_ext.i128        $95, $1
  mov.i64          $5, 1
.Lother:
  trap
  .end


hmul:
  .args            i64
  arg.i64          $1, 0
  mov.i64          $0, 2582544170319337227
  s_ext.i128        $2, $1
  s_ext.i128        $3, $0
  mul.i128         $4, $2, $3
  mov.i8           $5, 64
  srl.i128         $4, $4, $5
  trunc.i64        $0, $4
  ret.i64          $0
  .end


lmul:
  .args            i64

  arg.i64          $1, 0
  mov.i64          $0, 2582544170319337227
  mul.i64          $4, $1, $0
  ret.i64          $4
  .end


phi_loop:
  .call            caml
  .args            i64

  mov.i64          $1, 1
.L184.32:
  undef.i64        $5
  mov.i64          $65, 71
  cmp.lt.i32       $66, $5, $65
  jt               $66, .L194.32

  mov.i64          $69, 255
  cmp.lt.i32       $70, $5, $69
  jt               $70, .L186.32
.L194.32:

  mov.i64          $8, 2582544170319337227
  s_ext.i128        $95, $5
  s_ext.i128        $96, $8
  mul.i128         $97, $95, $96
  mov.i8           $98, 64
  srl.i128         $97, $97, $98
.L186.32:
  mov.i64          $3, 1
  cmp.ne.i32       $134, $5, $3
  jt               $134, .L184.32

  ret
  .end

