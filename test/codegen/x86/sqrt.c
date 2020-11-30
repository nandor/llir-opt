// RUN: %clang -O2 -target llir_x86_64 -S -xc - -o - | %opt - -triple x86_64 -o=-

float sqrtf(float x);
double sqrt(double x);
long double sqrtl(long double x);

float normf(float x, float y)
{
  // CHECK: sqrtss
  return sqrtf(x * x + y * y);
}

double norm(double x, double y)
{
  // CHECK: sqrtsd
  return sqrt(x * x + y * y);
}

long double norml(long double x, long double y)
{
  // CHECK: callq sqrt
  return sqrt(x * x + y * y);
}
