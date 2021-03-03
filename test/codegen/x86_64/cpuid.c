// RUN: %clang -O2 -target llir_x86_64 -S -xc - -o - | %opt - -triple x86_64 -o=-

#include <cpuid.h>

int test()
{
  unsigned int eax, ebx, ecx, edx;
  int v = __get_cpuid_count(0x100, 0x120, &eax, &ebx, &ecx, &edx);
  return v + eax + ebx + ecx + edx;
}
