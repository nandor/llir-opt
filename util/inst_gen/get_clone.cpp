// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "get_clone.h"
#include "util.h"

using ListInit = llvm::ListInit;
using StringInit = llvm::StringInit;
using DefInit = llvm::DefInit;



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
    OS << "Inst *CloneVisitor::Clone(" << type << "Inst *i) {";
    OS << "return new " << type << "Inst(";
    int numTypes = r->getValueAsInt("NumTypes");
    if (numTypes < 0) {
      OS << "std::vector<Type>{ i->type_begin(), i->type_end() },";
    } else {
      for (int i = 0; i < numTypes; ++i) {
        OS << "i->GetType(" << i << "), ";
      }
    }
    for (auto *field : r->getValueAsListOfDefs("Fields")) {
      if (field->getValueAsBit("IsList")) {
        if (field->getValueAsBit("IsScalar")) {
          OS << "i->Get" << field->getValueAsString("Name") << "()";
        } else {
          OS << "Map(i->" << field->getValueAsString("Name").lower() << "())";
        }
      } else {
        if (field->getValueAsBit("IsScalar")) {
          OS << "i->Get" << field->getValueAsString("Name") << "()";
        } else {
          OS << "Map(i->Get" << field->getValueAsString("Name") << "())";
        }
      }
      OS << ", ";
    }
    OS << "Annot(i));}\n";
  }

  OS << "#undef GET_CLONE_IMPL\n";
  OS << "#endif // GET_CLONE_IMPL\n";
}
