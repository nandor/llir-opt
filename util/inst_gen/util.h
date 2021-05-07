// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <llvm/TableGen/Record.h>



std::string GetTypeName(llvm::Record &r);
llvm::Record *GetBase(llvm::Record &r);


