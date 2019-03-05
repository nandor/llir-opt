// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <cstdint>



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
  std::size_t operator()(std::pair<T1, T2> const &p) const
  {
    std::size_t hash(0);
    ::hash_combine(hash, p.first);
    ::hash_combine(hash, p.second);
    return hash;
  }
};
