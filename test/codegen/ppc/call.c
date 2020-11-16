// RUN: %clang -target llir_x86_64-unknown-linux -xc - -S -O2 -o- | %opt - -triple x86_64-unknown-linux  -o=-
// DISABLED:

extern int test1_callee(int x, float y, int z);

int test1()
{
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
  return test2_callee(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
}
