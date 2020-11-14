// RUN: %clang -target llir_riscv64 -xc - -S -O2 -o- | %opt - -triple riscv64 -mcpu=sifive-u74 -o=-

typedef unsigned long __jmp_buf[2];

typedef struct __jmp_buf_tag {
  __jmp_buf __jb;
  unsigned long __fl;
  unsigned long __ss[128/sizeof(long)];
} jmp_buf[1];

int setjmp (jmp_buf);

extern int test();

int setjmp_test(int a, int b, int c) {
  jmp_buf buf;
  int q = test();
  if (setjmp(buf)) {
    return q + b + c + 5;
  } else {
    return a + b + 6;
  }
}
