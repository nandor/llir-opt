// RUN: %clang -O2 -target llir_aarch64 -S -xc - -o - | %opt - -triple aarch64 -o=-

int swap32(int *ptr)
{
  return __atomic_exchange_n(ptr, 0u, __ATOMIC_SEQ_CST);
}

long long swap64(long long *ptr)
{
  return __atomic_exchange_n(ptr, 0, __ATOMIC_SEQ_CST);
}
