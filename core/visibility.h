// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <cstdint>



/**
 * Enumeration of visibility settings.
 */
enum class Visibility : uint8_t {
  // Default visibility.
  DEFAULT,
  // External visibility.
  EXTERN,
  // Internal, hidden visibility.
  HIDDEN,
  // Weak, default visibility.
  WEAK_DEFAULT,
  // Weak, external visibility.
  WEAK_EXTERN,
  // Weak, hidden weak symbol.
  WEAK_HIDDEN,
};
