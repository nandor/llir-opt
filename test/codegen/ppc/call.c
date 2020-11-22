// RUN: %clang -target llir_powerpc64le-unknown-linux -xc - -S -O1 -o- | %opt - -triple powerpc64le-unknown-linux  -o=-

extern int test1_callee(int x, float y, int z);

int test1()
{
  // CHECK: -32(1)
  return test1_callee(1, 2.0f, 3);
}

extern int test2_callee(
    int x0,  int x1,  int x2,  int x3,
    int x4,  int x5,  int x6,  int x7,
    int x8,  int x9,  int x10, int x11,
    int x12, int x13, int x14, int x15
);

int test2()
{
  // CHECK: -176(1)
  return test2_callee(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
}
