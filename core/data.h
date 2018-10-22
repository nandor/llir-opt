// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <cstdint>



/**
 * The data segment of a program.
 */
class Data {
public:

  void Align(unsigned i);
  void AddInt8(int64_t i);
  void AddInt16(int64_t i);
  void AddInt32(int64_t i);
  void AddInt64(int64_t i);
  void AddSymbol(const char *sym);

private:
};
