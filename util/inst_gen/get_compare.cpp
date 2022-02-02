// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "get_compare.h"
#include "util.h"



// -----------------------------------------------------------------------------
void GetCompareWriter::run(llvm::raw_ostream &OS)
{
  OS << "#ifdef GET_COMPARE\n";
  OS << "#undef GET_COMPARE\n";

  for (auto r : records_.getAllDerivedDefinitions("Inst")) {
    if (r->getValueAsBit("HasCustomCompare")) {
      continue;
    }

    llvm::StringRef name(r->getName());
    auto type = GetTypeName(*r);
    OS << "case Inst::Kind::" << name << ": {\n";
    OS << "const auto &ai = static_cast<const " << type << " &>(a);\n";
    OS << "const auto &bi = static_cast<const " << type << " &>(b);\n";
    // Emit code to compare types.
    int numTypes = r->getValueAsInt("NumTypes");
    if (numTypes < 0) {
      OS << "if (ai.type_size() != bi.type_size()) return false;\n";
      OS << "for (unsigned i = 0, n = ai.type_size(); i < n; ++i) ";
      OS << "if (ai.type(i) != bi.type(i)) return false;\n";
    } else {
      for (int i = 0; i < numTypes; ++i) {
        OS << "if (ai.GetType(" << i << ") != bi.GetType(" << i << ")) ";
        OS << "return false;\n";
      }
    }

    // Emit code to read fields.
    auto fields = r->getValueAsListOfDefs("Fields");
    for (unsigned i = 0, n = fields.size(); i < n; ++i) {
      auto *field = fields[i];

      auto fieldType = field->getValueAsString("Type");
      auto fieldName = field->getValueAsString("Name");

      const bool isScalar = field->getValueAsBit("IsScalar");
      const bool isList = field->getValueAsBit("IsList");

      if (isScalar) {
        OS << "if (";
        OS << "ai.Get" << fieldName << "()";
        OS << " != ";
        OS << "bi.Get" << fieldName << "()";
        OS << ") return false;";
      } else {
        if (isList) {
          auto itName = llvm::StringRef(fieldName.lower()).drop_back().str();
          OS << "{";
          OS << "const size_t n = ai." << itName << "_size(); ";
          OS << "if (n != bi." << itName << "_size()) return false;";
          OS << "for (unsigned i = 0; i < n; ++i) ";
          OS << "if (!Equal(";
          OS << "ai." << itName << "(i), bi." << itName << "(i)";
          OS << ")) return false;";
          OS << "}";
        } else {
          OS << "if (!Equal(";
          OS << "ai.Get" << fieldName << "()";
          OS << ", ";
          OS << "bi.Get" << fieldName << "()";
          OS << ")) return false;";
        }
      }
      OS << "\n";
    }

    OS << "return true;\n}\n";
  }

  OS << "#endif // GET_COMPARE\n\n";
}
