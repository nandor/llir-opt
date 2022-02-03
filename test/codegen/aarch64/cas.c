// RUN: %clang -O2 -target llir_aarch64 -S -xc - -o - | %opt - -triple aarch64 -o=-

static inline int a_ll(volatile int *p)
{
  int v;
  __asm__ __volatile__
    ( "aarch64_load_link.i32 %w0, %1"
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
    ( "aarch64_store_cond.i32 %w0, %1, %w2"
    : "=r"(r)
    : "r"(p), "r"(v)
    : "memory"
    );
  return !r;
}

static inline void a_barrier()
{
  __asm__ __volatile__ ("aarch64_dmb" : : : "memory");
}


int wtf_cas(volatile int *p, int t, int s)
{
  // CHECK: ldaxr
  // CHECK: stlxr
  // CHECK: dmb
  int old;
  do {
    old = a_ll(p);
    if (old != t) {
      a_barrier();
      break;
    }
  } while (!a_sc(p, s));
  return old;
}
