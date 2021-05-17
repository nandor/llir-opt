// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "get_clone.h"
#include "util.h"



// -----------------------------------------------------------------------------
void GetCloneWriter::run(llvm::raw_ostream &OS)
{
  OS << "#ifdef GET_CLONE_IMPL\n";

  for (auto r : records_.getAllDerivedDefinitions("Inst")) {
    llvm::StringRef name(r->getName());

    if (r->getValueAsBit("HasCustomClone")) {
      continue;
    }

    auto type = GetTypeName(*r);
    OS << "Inst *CloneVisitor::Clone(" << type << " *inst) {";
    int numTypes = r->getValueAsInt("NumTypes");
    if (numTypes < 0) {
      OS << "std::vector<Type> types;\n";
      OS << "for (unsigned i = 0; i < inst->type_size(); ++i) ";
      OS << "types.push_back(Map(inst->type(i), inst, i));\n";
      OS << "return new " << type << "(types, ";
    } else {
      OS << "return new " << type << "(";
      for (int i = 0; i < numTypes; ++i) {
        OS << "Map(inst->GetType(" << i << "), inst, " << i << "), ";
      }
    }
    for (auto *field : r->getValueAsListOfDefs("Fields")) {
      if (field->getValueAsBit("IsList")) {
        if (field->getValueAsBit("IsScalar")) {
          OS << "inst->Get" << field->getValueAsString("Name") << "()";
        } else {
          OS << "Map(inst->" << field->getValueAsString("Name").lower() << "())";
        }
      } else {
        if (field->getValueAsBit("IsScalar")) {
          OS << "inst->Get" << field->getValueAsString("Name") << "()";
        } else {
          OS << "Map(inst->Get" << field->getValueAsString("Name") << "())";
        }
      }
      OS << ", ";
    }
    OS << "Annot(inst));}\n";
  }

  OS << "#undef GET_CLONE_IMPL\n";
  OS << "#endif // GET_CLONE_IMPL\n";
}
