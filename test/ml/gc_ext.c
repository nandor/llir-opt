// This file is part of the simd project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. ALl rights reserved.

#include <caml/alloc.h>
#include <caml/config.h>
#include <caml/fail.h>
#include <caml/memory.h>
#include <caml/mlvalues.h>




// -----------------------------------------------------------------------------
CAMLprim value gc_ident(value v)
{
  CAMLparam0();
  CAMLreturn(v);
}
