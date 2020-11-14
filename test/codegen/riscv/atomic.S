# RUN: %opt - -triple riscv64 -mfs +a

_xchg_64:
  .call            c
  mov.i64          $1, 0
  mov.i64          $2, var
  riscv_xchg.i64   $3, $2, $1
  ret              $3
  .end

_cmp_xchg_64:
  .call             c
  .args             i64, i64, i64
  arg.i64           $0, 0
  arg.i64           $1, 1
  arg.i64           $2, 2
  riscv_cmpxchg.i64 $3, $0, $1, $2
  ret               $3
  .end

_cmp_xchg_32:
  .call             c
  .args             i64, i32, i32
  arg.i64           $0, 0
  arg.i32           $1, 1
  arg.i32           $2, 2
  riscv_cmpxchg.i32 $3, $0, $1, $2
  ret               $3
  .end


  .section .data
var:
  .quad 0
