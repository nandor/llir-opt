// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <cstdint>
#include <utility>
#include <tuple>



/**
 * Custom hash combine.
 */
template <class T>
inline void hash_combine(std::size_t& seed, const T& v)
{
    std::hash<T> hasher;
    seed ^= hasher(v) + 0x9e3779b9 + (seed<<6) + (seed>>2);
}



/**
 * Hasher for std::pair structures.
 */
template<typename T1, typename T2>
struct std::hash<std::pair<T1, T2>> {
  std::size_t operator()(const std::pair<T1, T2> &p) const
  {
    std::size_t hash(0);
    ::hash_combine(hash, std::hash<T1>{}(p.first));
    ::hash_combine(hash, std::hash<T2>{}(p.second));
    return hash;
  }
};



/**
 * Hasher for std::tuple structures.
 */
template<typename... Ts>
class std::hash<std::tuple<Ts...>> {
private:
  template <size_t I, size_t N>
  static std::size_t Hash(const std::tuple<Ts...> &t)
  {
    if constexpr (I < N) {
      auto Elem = std::get<I>(t);
      std::size_t hash(0);
      ::hash_combine(hash, std::hash<decltype(Elem)>()(Elem));
      ::hash_combine(hash, Hash<I + 1, N>(t));
      return hash;
    } else {
      return std::size_t(0);
    }
  }

public:
  std::size_t operator()(const std::tuple<Ts...> &t) const
  {
    return Hash<0, std::tuple_size<std::tuple<Ts...>>::value>(t);
  }
};
