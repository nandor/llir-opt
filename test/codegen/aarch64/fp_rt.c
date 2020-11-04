// RUN: %clang -O2 -target llir_aarch64 -S -xc - -o - | %opt - -triple aarch64 -o=-

#include <stdarg.h>

union arg
{
  long double f;
};

void pop_arg(union arg *arg, int type, va_list *ap)
{
  arg->f = va_arg(*ap, double);
}
