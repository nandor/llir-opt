// RUN: %clang -target llir_x86_64 -S -xc - -o - | %opt - -triple x86_64 -o=-


typedef unsigned long long uint64_t;
typedef unsigned __int128 uint128_t;

uint64_t multiply(uint64_t x, uint64_t y)
{
  return (((uint128_t) x) * ((uint128_t) y)) >> 70ull;
}
