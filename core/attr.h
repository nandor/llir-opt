// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once


/**
 * Enumeration of supported calling conventions.
 */
enum class CallingConv {
  // Generic C calling convention.
  C,
  // Fast C calling convention.
  FAST,
  // Generic OCaml calling convention.
  OCAML,
  // OCaml external call.
  EXT
};

/**
 * Enumeration of visibility settings.
 */
enum class Visibility {
  // External visibility.
  EXTERN,
  // Internal, hidden visibility.
  HIDDEN,
};
