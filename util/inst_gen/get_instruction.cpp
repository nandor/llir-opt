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


  OS << "#ifdef GET_KIND\n";
  auto *inst = records_.getClass("Inst");
  for (const auto &[name, c] : records_.getClasses()) {
    auto *base = c.get();
    if (!base->isSubClassOf(inst)) {
      continue;
    }
    auto *parent = GetBase(*base);
    OS << "GET_KIND(" << name << ", " << parent->getName() << ")\n";
  }
  OS << "#undef GET_KIND\n";
  OS << "#endif // GET_KIND\n\n";
}
