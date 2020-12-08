// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/StringRef.h>

#include "util.h"



// -----------------------------------------------------------------------------
static std::string ToCamelCase(llvm::StringRef name)
{
  bool upper = true;
  std::string str;
  for (size_t i = 0, n = name.size(); i < n; ++i) {
    if (name[i] == '_') {
      upper = true;
    } else if (upper) {
      str += toupper(name[i]);
      upper = false;
    } else {
      str += tolower(name[i]);
      upper = false;
    }
  }
  return str;
}

// -----------------------------------------------------------------------------
std::string GetTypeName(llvm::Record &r)
{
  llvm::StringRef name(r.getName());
  auto type = r.getValueAsString("Type");
  if (type.empty()) {
    if (name.startswith("X86_")) {
      return "X86_" + ToCamelCase(name.substr(4));
    }
    if (name.startswith("AARCH64_")) {
      return "AArch64_" + ToCamelCase(name.substr(8));
    }
    if (name.startswith("RISCV_")) {
      return "RISCV_" + ToCamelCase(name.substr(6));
    }
    if (name.startswith("PPC_")) {
      return "PPC_" + ToCamelCase(name.substr(4));
    }
    return ToCamelCase(name);
  } else {
    return type.str();
  }
}
