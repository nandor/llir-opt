// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "get_instruction.h"
#include "util.h"



// -----------------------------------------------------------------------------
void GetInstructionWriter::run(llvm::raw_ostream &OS)
{
  OS << "#ifdef GET_INST\n";
  for (auto R : records_.getAllDerivedDefinitions("Inst")) {
    llvm::StringRef name(R->getName());

    OS << "GET_INST(";
    OS << name << ", ";
    OS << GetTypeName(*R) << ", ";
    OS << "\"" << name.lower() << "\", ";
    OS << R->getType()->getAsString();
    OS << ")\n";
  }
  OS << "#undef GET_INST\n";
  OS << "#endif // GET_INST\n\n";
}
