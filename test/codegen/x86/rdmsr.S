# RUN: %opt - -triple x86_64


# CHECK: rd
test_rd:
  mov.i32            $0, 0xc0000100
  x86_rd_msr.i64.i64 $1, $2, $0
  add.i64            $3, $1, $2
  ret                $3
