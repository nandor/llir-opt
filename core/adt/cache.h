// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "core/adt/hash.h"

/**
 * Generic cache with a method to build new elements.
 */
template <typename T, typename... Args>
class Cache {
private:
  using KeyT = std::tuple<Args...>;

public:
  T operator() (Args... args, std::function<T()> &&f)
  {
    KeyT key = {args...};
    if (auto it = cache_.find(key); it != cache_.end()) {
      return it->second;
    }

    T result = f();
    cache_.emplace(key, result);
    return result;
  }

private:
  std::unordered_map<KeyT, T> cache_;
};
