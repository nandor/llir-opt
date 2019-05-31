// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <cstdint>



/**
 * Node identifier type.
 */
template<typename T>
class ID final {
public:
  /// Creates a new item.
  ID(uint32_t id) : id_(id) { }

  /// Returns the ID.
  operator uint32_t () const { return id_; }

private:
  /// ID of the item.
  uint32_t id_;
};
