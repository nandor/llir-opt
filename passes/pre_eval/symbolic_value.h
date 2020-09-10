// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <llvm/ADT/APInt.h>



/**
 * Symbolic representation of a value.
 */
class SymbolicValue final {
public:
  enum class Kind {
    INT,
    FLOAT,
    ATOM,
    FUNC,
    BLOCK,
    EXTERN,
    UNKNOWN,
    UNDEFINED,
  };

public:

  Kind GetKind() const;

private:

};
