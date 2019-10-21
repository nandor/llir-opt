// This file is part of the genm project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. ALl rights reserved.

#include <assert.h>
#include <math.h>
#include <pmmintrin.h>

#include <caml/alloc.h>
#include <caml/config.h>
#include <caml/fail.h>
#include <caml/memory.h>
#include <caml/mlvalues.h>



/// SIMD value type.
typedef union {
  struct {
    float x;
    float y;
    float z;
    float w;
  };
} simd_t;


/// Tag of the complex blocks.
#define SIMD_TAG Abstract_tag
/// Size of the complex blocks.
#define SIMD_SIZE ((size_t)sizeof(simd_t))
/// Accessor for the complex value.
#define Simd_val(block) ((simd_t *)Op_val(block))

/// Make sure we can use AllocSmall with the complex blocks.
static_assert(SIMD_SIZE < Max_young_wosize, "Invalid size");


// -----------------------------------------------------------------------------
CAMLprim value simd_make(value x, value y, value z, value w)
{
  CAMLparam4(x, y, z, w);
  CAMLlocal1(block);

  block = caml_alloc_small(SIMD_SIZE, SIMD_TAG);
  simd_t *r = Simd_val(block);
  r->x = Double_val(x);
  r->y = Double_val(y);
  r->z = Double_val(z);
  r->w = Double_val(w);
  CAMLreturn(block);
}

// -----------------------------------------------------------------------------
CAMLprim value simd_scale(value v, value s)
{
  CAMLparam2(v, s);
  CAMLlocal1(block);

  simd_t *vec = Simd_val(v);
  const double scale = Double_val(s);

  block = caml_alloc_small(SIMD_SIZE, SIMD_TAG);
  simd_t *r = Simd_val(block);
  r->x = vec->x * scale;
  r->y = vec->y * scale;
  r->z = vec->z * scale;
  r->w = vec->w * scale;
  CAMLreturn(block);
}

// -----------------------------------------------------------------------------
CAMLprim value simd_dot(value a, value b)
{
  CAMLparam2(a, b);

  simd_t *vec_a = Simd_val(a);
  simd_t *vec_b = Simd_val(b);

  float dot = 0.0f;
  dot += vec_a->x * vec_b->x;
  dot += vec_a->y * vec_b->y;
  dot += vec_a->z * vec_b->z;
  dot += vec_a->w * vec_b->w;
  CAMLreturn(caml_copy_double(dot));
}
