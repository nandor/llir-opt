// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/error.h"



// -----------------------------------------------------------------------------
llvm::Error MakeError(llvm::Twine msg)
{
  return llvm::make_error<llvm::StringError>(msg, llvm::inconvertibleErrorCode());
}
