// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <cstdint>



/**
 * Enumeration of supported calling conventions.
 */
enum class CallingConv : uint8_t {
  // Generic C calling convention.
  C,
  // Fast C calling convention.
  FAST,
  // Generic OCaml calling convention.
  CAML,
  // OCaml allocator call.
  CAML_ALLOC,
  // OCaml gc trampoline.
  CAML_GC,
  // OCaml exception call.
  CAML_RAISE,
};

/**
 * Enumeration of visibility settings.
 */
enum class Visibility : uint8_t {
  // External visibility.
  EXTERN,
  // Internal, hidden visibility.
  HIDDEN,
};
