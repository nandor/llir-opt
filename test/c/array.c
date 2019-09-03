#include <stdio.h>
#include <stdint.h>


int64_t __attribute__((noinline)) aggregate(
  unsigned n,
  int64_t a[],
  int64_t b[],
  int64_t c[])
{
  int64_t sum = 0;
  for (unsigned i = 0; i < n; ++i) {
    sum += a[i];
    sum += b[i];
    sum += c[i];
  }
  return sum;
}


int64_t __attribute__((noinline)) compute(int x)
{
  int64_t a[] = { x, 1 };
  int64_t b[] = { x, 2 };
  int64_t c[] = { x, 3 };
  return aggregate(2, a, b, c);
}


int main()
{
  return compute(1) - 9;
}
