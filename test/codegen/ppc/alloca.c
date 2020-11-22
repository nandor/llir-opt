// RUN: %clang -target llir_powerpc64le-unknown-linux -xc - -S -O2 -o- | %opt - -triple powerpc64le-unknown-linux  -o=-


extern void sink(int *x);


void test(int n, int y)
{
  int temp[n];
  temp[n - 1] = y;
  sink(temp);
}
