// This file is part of the simd project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. ALl rights reserved.

#include <caml/alloc.h>
#include <caml/config.h>
#include <caml/fail.h>
#include <caml/memory.h>
#include <caml/mlvalues.h>



// -----------------------------------------------------------------------------
CAMLprim value exc_throw(value n)
{
  CAMLparam0();

  caml_raise_not_found();

  CAMLreturn(Val_unit);
}

// -----------------------------------------------------------------------------
CAMLprim value exc_get_0(value unit)
{
  CAMLparam0();
  CAMLreturn(Val_long(0));
}
