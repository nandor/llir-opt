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
  float v[4];
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
  simd_t *v = Simd_val(block);
  v->x = Double_val(x);
  v->y = Double_val(y);
  v->z = Double_val(z);
  v->w = Double_val(w);
  CAMLreturn(block);
}

// -----------------------------------------------------------------------------
CAMLprim value simd_scale(value v, value s)
{
  CAMLparam2(v, s);
  CAMLlocal1(block);

  __m128 vec_v = _mm_loadu_ps(Simd_val(v)->v);
  __m128 vec_s = _mm_set1_ps(Double_val(s));

  block = caml_alloc_small(SIMD_SIZE, SIMD_TAG);
  _mm_storeu_ps(Simd_val(block)->v, _mm_mul_ps(vec_v, vec_s));

  CAMLreturn(block);
}

// -----------------------------------------------------------------------------
CAMLprim value simd_dot(value a, value b)
{
  CAMLparam2(a, b);

  __m128 vec_a = _mm_loadu_ps(Simd_val(a)->v);
  __m128 vec_b = _mm_loadu_ps(Simd_val(b)->v);

  __m128 mulReg, shufReg, sumsReg;

  mulReg = _mm_mul_ps(vec_a, vec_b);
  shufReg = _mm_movehdup_ps(mulReg);
  sumsReg = _mm_add_ps(mulReg, shufReg);
  shufReg = _mm_movehl_ps(shufReg, sumsReg);
  sumsReg = _mm_add_ss(sumsReg, shufReg);

  CAMLreturn(caml_copy_double(_mm_cvtss_f32(sumsReg)));
}
