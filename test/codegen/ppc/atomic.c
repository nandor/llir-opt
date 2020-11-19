// RUN: %clang -O2 -target llir_powerpc64le -S -xc - -o - | %opt - -triple powerpc64le -o=-

int swap32(int *ptr)
{
  // CHECK: lwarx
  // CHECK: stwcx
  return __atomic_exchange_n(ptr, 0u, __ATOMIC_SEQ_CST);
}

long long swap64(long long *ptr)
{
  // CHECK: ldarx
  // CHECK: stdcx
  return __atomic_exchange_n(ptr, 0, __ATOMIC_SEQ_CST);
}
