// RUN: %clang -O2 -target llir_powerpc64le -S -xc - -o - | %opt - -triple powerpc64le -o=-

extern int q(int, int, int);

static int
__attribute__((noinline))
fact(int x, int y, int z)
{
  return q(x, y, z) + 5;
}


int test_call(int y)
{
  // CHECK: b fact
  return fact(y + 5, y * 6, 123);
}
