// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "get_instruction.h"


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
static std::string GetTypeName(llvm::StringRef name)
{
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
}

// -----------------------------------------------------------------------------
void GetInstructionWriter::run(llvm::raw_ostream &OS)
{
  OS << "#ifdef GET_INST\n";
  for (auto R : records_.getAllDerivedDefinitions("Inst")) {
    llvm::StringRef name(R->getName());

    auto type = llvm::dyn_cast<llvm::StringInit>(
        R->getValue("Type")->getValue()
    )->getValue();

    OS << "GET_INST(";
    OS << name << ", ";
    if (type.empty()) {
      OS << GetTypeName(name) << ", ";
    } else {
      OS << type << ", ";
    }
    OS << "\"" << name.lower() << "\", ";
    OS << R->getType()->getAsString();
    OS << ")\n";
  }
  OS << "#undef GET_INST\n";
  OS << "#endif // GET_INST\n\n";
}
