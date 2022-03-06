// RUN: %clang -O2 -target llir_x86_64 -S -xc++ - -o - -O1

#include <assert.h>

int __attribute__((noinline)) test()
{
  throw 1;
}

int main(int argc, char **argv)
{
  try {
    test();
    __builtin_trap();
  } catch (int n) {
    if (n != 1) {
      __builtin_trap();
    }
    return 0;
  }
}
