// RUN: %clang -O2 -target llir_x86_64 -S -xc - -o -


int
__attribute__((noinline))
a()
{
  return 0;
}

// CHECK: .set b, a
int b() __attribute__((alias("a")));

int c()
{
  return b();
}
