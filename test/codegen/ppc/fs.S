# RUN: %opt - -triple powerpc64le

__init_ssp:
  .visibility          global_default
  .args                i64
  .call                c

  mov.i64              $13, 0
  mov.i64              $11, 40
  # CHECK:             13
  mov.i64              $10, $fs
  add.i64              $12, $10, $11
  st                   [$12], $13
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

