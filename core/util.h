// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <llvm/ADT/StringRef.h>

class Prog;



std::unique_ptr<Prog> Parse(llvm::StringRef buffer);
