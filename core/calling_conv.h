// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <cstdint>

#include <llvm/Support/raw_ostream.h>



/**
 * Enumeration of supported calling conventions.
 */
enum class CallingConv : uint8_t {
  // Generic C calling convention.
  C,
  // Generic OCaml calling convention.
  CAML,
  // OCaml allocator call.
  CAML_ALLOC,
  // OCaml gc trampoline.
  CAML_GC,
  // Setjmp convention.
  SETJMP,
  // Xen hypervisor call.
  XEN,
  // Interrupt handler.
  INTR,
  // Multiboot entry.
  MULTIBOOT,
};

/**
 * Prints the calling convention to a stream.
 */
llvm::raw_ostream &operator<<(llvm::raw_ostream &os, CallingConv conv);
