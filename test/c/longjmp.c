#include <setjmp.h>

jmp_buf buffer;


void callee(int *a, int *b, int *c)
{
  *a = 1;
  *b = 2;
  *c = 3;
  longjmp(buffer, 1);
}

int main()
{
  volatile int a = 0;
  volatile int b = 0;
  volatile int c = 0;

  if (setjmp(buffer)) {
    return a + b + c - 6;
  } else {
    callee(&a, &b, &c);
    return -1;
  }
}
