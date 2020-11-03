// RUN: %clang -target llir_x86_64 -S -xc - -o - | %opt - -triple x86_64 -o=-


int a()
{
  // CHECK: rdtsc
  return __rdtsc();
}

int b()
{
  // CHECK: rdtsc
  return __builtin_readcyclecounter();
}
