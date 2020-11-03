// RUN: %clang -O2 -target llir_x86_64 -S -xc - -o - | %opt - -triple x86_64 -o=-

long long swap(long long *ptr)
{
  return __atomic_exchange_n(ptr, 0, __ATOMIC_SEQ_CST);
}
