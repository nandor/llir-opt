// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <cstdint>



/**
 * Symbol visibility options.
 *
 * Combines ELF bindings and ELF visibility attributes, including only the
 * combinations which are valid.
 *
 * The supported bindings are:
 * - local
 * - global
 * - weak
 *
 * The supported visibility levels are:
 * - default
 * - hidden
 */
enum class Visibility : uint8_t {
  /// Local visibility, private to the compilation unit.
  LOCAL,
  /// Global visibility.
  ///
  /// Equivalent to ELF global + default.
  GLOBAL_DEFAULT,
  /// Hidden global.
  ///
  /// Equivalent to ELF global + hidden.
  GLOBAL_HIDDEN,
  /// Weak symbol with default visibility.
  ///
  /// Equivalent to ELF weak + default.
  WEAK_DEFAULT,
  /// Weak symbol with hidden visibility.
  ///
  /// Equivalent to ELF weak + hidden.
  WEAK_HIDDEN,
};
