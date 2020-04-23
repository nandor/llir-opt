// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <cstdint>


/**
 * Condition flag.
 */
enum class Cond : uint8_t {
  EQ, OEQ, UEQ,
  NE, ONE, UNE,
  LT, OLT, ULT,
  GT, OGT, UGT,
  LE, OLE, ULE,
  GE, OGE, UGE,
};
