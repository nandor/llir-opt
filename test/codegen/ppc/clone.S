# RUN: %opt - -triple=powerpc64le

  .section .text
__clone:
  .call       c
  .args       i64, i64, i64, i64, i64, i64, i64
  .noinline
  arg.i64     $0, 0 # callee
  arg.i64     $1, 1 # stack
  arg.i64     $2, 2 # flags
  arg.i64     $3, 3 # arg
  arg.i64     $4, 4 # ptid
  arg.i64     $5, 5 # tls
  arg.i64     $6, 6 # ctid
  clone.i64   $7, $0, $1, $2, $3, $4, $5, $6
  ret         $7

