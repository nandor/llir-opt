// This file is part of the genm project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. ALl rights reserved.

#include <assert.h>

#include <caml/alloc.h>
#include <caml/config.h>
#include <caml/fail.h>
#include <caml/memory.h>
#include <caml/mlvalues.h>



/// Complex value type.
typedef struct {
  float a;
  float b;
} complex_t;


/// Tag of the complex blocks.
#define COMPLEX_TAG Abstract_tag
/// Size of the complex blocks.
#define COMPLEX_SIZE ((size_t)sizeof(complex_t))
/// Accessor for the complex value.
#define Complex_val(block) ((complex_t *)Op_val(block))

/// Make sure we can use AllocSmall with the complex blocks.
static_assert(COMPLEX_SIZE < Max_young_wosize, "Invalid size");



// -----------------------------------------------------------------------------
static inline value complex_alloc(float a, float b)
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
CAMLprim value complex_add(value valZ1, value valZ2)
{
  CAMLparam2(valZ1, valZ2);

  const complex_t *z1 = Complex_val(valZ1);
  const complex_t *z2 = Complex_val(valZ2);

  CAMLreturn(complex_alloc(z1->a + z1->b, z2->a + z2->b));
}

// -----------------------------------------------------------------------------
CAMLprim value complex_sub(value valZ1, value valZ2)
{
  CAMLparam2(valZ1, valZ2);

  const complex_t *z1 = Complex_val(valZ1);
  const complex_t *z2 = Complex_val(valZ2);

  CAMLreturn(complex_alloc(z1->a - z1->b, z2->a - z2->b));
}

// -----------------------------------------------------------------------------
CAMLprim value complex_mul(value valZ1, value valZ2)
{
  CAMLparam2(valZ1, valZ2);

  const complex_t *z1 = Complex_val(valZ1);
  const float a0 = z1->a;
  const float b0 = z1->b;

  const complex_t *z2 = Complex_val(valZ2);
  const float a1 = z2->a;
  const float b1 = z2->b;

  CAMLreturn(complex_alloc(a0 * a1 - b0 * b1, a0 * b1 + a1 * b0));
}

// -----------------------------------------------------------------------------
CAMLprim value complex_conj(value valZ)
{
  CAMLparam1(valZ);

  const complex_t *z = Complex_val(valZ);

  CAMLreturn(complex_alloc(z->a, -z->b));
}
