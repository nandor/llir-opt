# RUN: %opt - -triple riscv64 -mcpu sifive-u74

__init_ssp:
  .visibility          global_default
  .args                i64
  .call                c

  mov.i64              $13, 0
  mov.i64              $11, 40
  # CHECK:             tp
  get.i64              $10, $fs
  add.i64              $12, $10, $11
  store                   [$12], $13
  ret
  .end

  .section .text
  .globl __set_thread_area
  .hidden __set_thread_area
__set_thread_area:
  .call       c
  .args       i64

  arg.i64     $0, 0
  set         $fs, $0
  mov.i64     $1, 0
  ret         $1
  .end
