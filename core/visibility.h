// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <cstdint>



/**
 * Enumeration of visibility settings.
 */
enum class Visibility : uint8_t {
  // External visibility.
  EXTERN,
  // Internal, hidden visibility.
  HIDDEN,
  // Weak symbol.
  WEAK,
  // Exported from a shared object.
  EXPORT
};
