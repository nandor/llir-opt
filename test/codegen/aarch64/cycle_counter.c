// RUN: %clang -target llir_aarch64 -S -xc - -o - | %opt - -triple aarch64 -o=-

int b()
{
  // CHECK: mrs
  return __builtin_readcyclecounter();
}
