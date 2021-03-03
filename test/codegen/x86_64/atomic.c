// RUN: %clang -O2 -target llir_x86_64 -S -xc - -o - | %opt - -triple x86_64 -o=-

long long swap(long long *ptr)
{
  return __atomic_exchange_n(ptr, 0, __ATOMIC_SEQ_CST);
}

long long load64(long long *ptr)
{
  return __atomic_load_n(ptr, __ATOMIC_ACQUIRE);
}

void store64(long long *ptr, long long val)
{
  __atomic_store_n(ptr, val, __ATOMIC_RELEASE);
}

int load32(int *ptr)
{
  return __atomic_load_n(ptr, __ATOMIC_ACQUIRE);
}

void store32(int *ptr, int val)
{
  __atomic_store_n(ptr, val, __ATOMIC_RELEASE);
}

long long or8(char *ptr, char val)
{
  return __atomic_fetch_or(ptr, val, __ATOMIC_SEQ_CST);
}

long long or32(int *ptr, int val)
{
  return __atomic_fetch_or(ptr, val, __ATOMIC_SEQ_CST);
}

long long or64(long long *ptr, long long val)
{
  return __atomic_fetch_or(ptr, val, __ATOMIC_SEQ_CST);
}

long long and32(int *ptr, int val)
{
  return __atomic_fetch_and(ptr, val, __ATOMIC_SEQ_CST);
}

long long and64(long long *ptr, long long val)
{
  return __atomic_fetch_and(ptr, val, __ATOMIC_SEQ_CST);
}
