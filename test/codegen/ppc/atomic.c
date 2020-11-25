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

static inline int a_ll(volatile int *p)
{
  int v;
  __asm__ __volatile__
    ( "ppc_ll.i32 %0, %1"
    : "=r"(v)
    : "r"(p)
    :
    );
  return v;
}

static inline int a_sc(volatile int *p, int v)
{
  int r;
  __asm__ __volatile__
    ( "ppc_sc.i32 %0, %1, %2"
    : "=r"(r)
    : "r"(p), "r"(v)
    : "memory"
    );
  return r;
}

int a_swap(volatile int *p, int v)
{
  int old;
  do old = a_ll(p);
  while (!a_sc(p, v));
  __asm__ __volatile__ ("ppc_isync" : : : "memory");
  return old;
}
