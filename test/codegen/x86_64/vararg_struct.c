// RUN: %clang -O2 -target llir_x86_64 -S -xc - -o - | %opt - -triple x86_64 -o=-

typedef __builtin_va_list va_list;

#define va_start(v,l)   __builtin_va_start(v,l)
#define va_end(v)       __builtin_va_end(v)
#define va_arg(v,l)     __builtin_va_arg(v,l)
#define va_copy(d,s)    __builtin_va_copy(d,s)

int printf(const char *__restrict, ...);

struct test {
  int x;
  int y;
  int z;
};

void
vararg_one(int n, ...)
{
  va_list args;
  va_start(args, n);
  struct test t = va_arg(args, struct test);
  printf("%d %d %d\n", t.x, t.y, t.z);
  va_end(args);
}

void
__attribute__((noinline))
vararg_callee(int n, ...)
{
  va_list args;
  va_start(args, n);
  for (int x = 0; x < n; x++) {
    struct test t = va_arg(args, struct test);
    printf("%d %d %d\n", t.x, t.y, t.z);
  }
  va_end(args);
}
void
__attribute__((noinline))
fixed_callee(
    struct test t0,
    struct test t1,
    struct test t2,
    struct test t3,
    struct test t4,
    struct test t5)
{
    printf("%d %d %d\n", t0.x, t0.y, t0.z);
    printf("%d %d %d\n", t1.x, t1.y, t1.z);
    printf("%d %d %d\n", t2.x, t2.y, t2.z);
    printf("%d %d %d\n", t3.x, t3.y, t3.z);
    printf("%d %d %d\n", t4.x, t4.y, t4.z);
    printf("%d %d %d\n", t5.x, t5.y, t5.z);
}

int
__attribute__((noinline))
vararg_integer(int n, ...)
{
  va_list args;
  va_start(args, n);
  int sum = 0;
  for (int x = 0; x < n; x++) {
    sum += va_arg(args, int);
  }
  va_end(args);
  return sum;
}

int main() {
  struct test t0 = { .x = 10, .y = 20, .z = 30 };
  struct test t1 = { .x = 11, .y = 21, .z = 31 };
  struct test t2 = { .x = 12, .y = 22, .z = 32 };
  struct test t3 = { .x = 13, .y = 23, .z = 33 };
  struct test t4 = { .x = 14, .y = 24, .z = 34 };
  struct test t5 = { .x = 15, .y = 25, .z = 35 };
  fixed_callee(t0, t1, t2, t3, t4, t5);
  vararg_callee(6, t0, t1, t2, t3, t4, t5);
  return 0;
}
