// This file is part of the genm project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. ALl rights reserved.

#include <assert.h>
#include <math.h>

#include <caml/alloc.h>
#include <caml/config.h>
#include <caml/fail.h>
#include <caml/memory.h>
#include <caml/mlvalues.h>



/// Complex value type.
typedef struct {
  double a;
  double b;
} complex_t;


/// Tag of the complex blocks.
#define COMPLEX_TAG Abstract_tag
/// Size of the complex blocks.
#define COMPLEX_SIZE ((size_t)sizeof(complex_t))
/// Accessor for the complex value.
#define Complex_val(block) ((complex_t *)Op_val(block))

/// Make sure we can use AllocSmall with the complex blocks.
static_assert(COMPLEX_SIZE < Max_young_wosize, "Invalid size");

/// Tag of the complex blocks.
#define VEC_TAG Abstract_tag
/// Accessor for the complex value.
#define Vec_val(block) ((complex_t *)Op_val(block))



// -----------------------------------------------------------------------------
static inline value complex_alloc(double a, double b)
{
  value block = caml_alloc_small(COMPLEX_SIZE, COMPLEX_TAG);
  complex_t *z = Complex_val(block);
  z->a = a;
  z->b = b;
  return block;
}

// -----------------------------------------------------------------------------
CAMLprim value complex_make(value a, value b)
{
  CAMLparam2(a, b);
  CAMLreturn(complex_alloc(Double_val(a), Double_val(b)));
}

// -----------------------------------------------------------------------------
CAMLprim value complex_re(value valZ)
{
  CAMLparam1(valZ);
  CAMLreturn(caml_copy_double(Complex_val(valZ)->a));
}

// -----------------------------------------------------------------------------
CAMLprim value complex_im(value valZ)
{
  CAMLparam1(valZ);
  CAMLreturn(caml_copy_double(Complex_val(valZ)->b));
}

// -----------------------------------------------------------------------------
CAMLprim value complex_add(value valZ1, value valZ2)
{
  CAMLparam2(valZ1, valZ2);

  const complex_t *z1 = Complex_val(valZ1);
  const complex_t *z2 = Complex_val(valZ2);

  CAMLreturn(complex_alloc(z1->a + z2->a, z1->b + z2->b));
}

// -----------------------------------------------------------------------------
CAMLprim value complex_sub(value valZ1, value valZ2)
{
  CAMLparam2(valZ1, valZ2);

  const complex_t *z1 = Complex_val(valZ1);
  const complex_t *z2 = Complex_val(valZ2);

  CAMLreturn(complex_alloc(z1->a - z2->a, z1->b - z2->b));
}

// -----------------------------------------------------------------------------
CAMLprim value complex_mul(value valZ1, value valZ2)
{
  CAMLparam2(valZ1, valZ2);

  const complex_t *z1 = Complex_val(valZ1);
  const double a0 = z1->a;
  const double b0 = z1->b;

  const complex_t *z2 = Complex_val(valZ2);
  const double a1 = z2->a;
  const double b1 = z2->b;

  CAMLreturn(complex_alloc(a0 * a1 - b0 * b1, a0 * b1 + a1 * b0));
}

// -----------------------------------------------------------------------------
CAMLprim value complex_exp(value valZ)
{
  CAMLparam1(valZ);

  const complex_t *z = Complex_val(valZ);
  const double a = z->a;
  const double b = z->b;

  const double ea = exp(a);

  CAMLreturn(complex_alloc(ea * cos(b), ea * sin(b)));
}


// -----------------------------------------------------------------------------
CAMLprim value complex_abs(value valZ)
{
  CAMLparam1(valZ);

  const complex_t *z = Complex_val(valZ);
  CAMLreturn(caml_copy_double(sqrt(z->a * z->a + z->b * z->b)));
}

// -----------------------------------------------------------------------------
CAMLprim value complex_vec_make(value len)
{
  CAMLparam1(len);
  CAMLlocal1(result);

  const uintnat n = Long_val(len);
  result = caml_alloc(n * 2, Double_array_tag);

  complex_t *ptr = Vec_val(result);
  for (uintnat i = 0; i < n; ++i) {
    ptr[i].a = 0.0;
    ptr[i].b = 0.0;
  }

  CAMLreturn(result);
}

// -----------------------------------------------------------------------------
CAMLprim value complex_vec_length(value v)
{
  CAMLparam1(v);
  CAMLreturn(Val_long(Wosize_hd(Hd_val(v)) >> 1));
}

// -----------------------------------------------------------------------------
CAMLprim value complex_vec_unsafe_get(value v, value i)
{
  CAMLparam2(v, i);
  complex_t *ptr = Vec_val(v);
  uintnat idx = Long_val(i);
  CAMLreturn(complex_alloc(ptr[idx].a, ptr[idx ].b));
}

// -----------------------------------------------------------------------------
CAMLprim value complex_vec_unsafe_set(value v, value i, value valZ)
{
  CAMLparam3(v, i, valZ);

  const complex_t *z = Complex_val(valZ);
  uintnat idx = Long_val(i);

  complex_t *ptr = Vec_val(v);
  ptr[idx].a = z->a;
  ptr[idx].b = z->b;

  CAMLreturn(Val_unit);
}
