// RUN: %clang -target llir_powerpc64le -xc - -S -o- -O1 | %opt - -triple powerpc64le -o=-

typedef __builtin_va_list va_list;

#define va_start(v,l)   __builtin_va_start(v,l)
#define va_end(v)       __builtin_va_end(v)
#define va_arg(v,l)     __builtin_va_arg(v,l)
#define va_copy(d,s)    __builtin_va_copy(d,s)

double averagef(int num, ...)
{
  va_list args;
  va_start(args, num);

  double sum = 0;
  for (int x = 0; x < num; x++) {
    sum += va_arg(args, double);
  }
  va_end(args);
  return sum / num;
}

int averagei(int num, ...)
{
  va_list args;
  va_start(args, num);

  int sum = 0;
  for (int x = 0; x < num; x++) {
    sum += va_arg(args, int);
  }
  va_end(args);
  return sum / num;
}
