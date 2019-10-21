// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <cstdint>

#include "core/adt/hash.h"



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


/**
 * Hash for the ID.
 */
template<typename T>
struct std::hash<ID<T>> {
  std::size_t operator()(const ID<T> &p) const
  {
    return std::hash<uint32_t>{}(static_cast<uint32_t>(p));
  }
};
