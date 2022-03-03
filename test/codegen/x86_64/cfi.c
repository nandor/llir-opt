// RUN: %clang -O2 -target llir_x86_64 -S -xc - -o - -O3 | %opt - -triple x86_64 -o=- -emit=asm

#include <stdlib.h>


// CHECK: .cfi_startproc
// CHECK: .cfi_endproc
int __attribute__((noinline)) f(int *a, int *b, int n)
{
  int sum = 0;
  a[0] = 0;
  for (unsigned i = 1; i < n; ++i) {
    a[i] = b[i - 1] + a[i - 1];
    sum += a[i];
  }
  return sum;
}

// CHECK: .cfi_startproc
// CHECK: .cfi_def_cfa_offset 16
// CHECK: .cfi_offset %rbp, -16
// CHECK: .cfi_def_cfa_register %rbp
// CHECK: .cfi_def_cfa %rsp, 8
// CHECK: .cfi_endproc
int test(int *b, int n) {
  int *a = (int*)alloca(n * sizeof(int));
  return f(a, b, n);
}
