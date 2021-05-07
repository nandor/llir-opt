// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/Twine.h>

#include "util.h"



// -----------------------------------------------------------------------------
static std::string ToCamelCase(llvm::StringRef name)
{
  bool upper = true;
  std::string str;
  for (auto c : name) {
    if (c == '_') {
      upper = true;
    } else if (upper) {
      str += toupper(c);
      upper = false;
    } else {
      str += tolower(c);
      upper = false;
    }
  }
  return str;
}

// -----------------------------------------------------------------------------
std::string GetTypeName(llvm::Record &r)
{
  llvm::StringRef name(r.getName());
  if (r.isClass()) {
    return name.str();
  }
  if (name.startswith("X86_")) {
    return "X86_" + ToCamelCase(name.substr(4)) + "Inst";
  }
  if (name.startswith("AARCH64_")) {
    return "AArch64_" + ToCamelCase(name.substr(8)) + "Inst";
  }
  if (name.startswith("RISCV_")) {
    return "RISCV_" + ToCamelCase(name.substr(6)) + "Inst";
  }
  if (name.startswith("PPC_")) {
    return "PPC_" + ToCamelCase(name.substr(4)) + "Inst";
  }
  return ToCamelCase(name) + "Inst";
}

// -----------------------------------------------------------------------------
llvm::Record *GetBase(llvm::Record &r)
{
  auto bases = r.getType()->getClasses();
  assert(bases.size() == 1 && "single base expected");
  return bases[0];
}


