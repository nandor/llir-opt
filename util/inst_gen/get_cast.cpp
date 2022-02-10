// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "get_cast.h"
#include "util.h"


// -----------------------------------------------------------------------------
void GetCastWriter::run(llvm::raw_ostream &OS)
{
  OS << "#ifdef GET_CAST_INTF\n";
  OS << "#undef GET_CAST_INTF\n";
  auto *inst = records_.getClass("Inst");
  for (const auto &[name, c] : records_.getClasses()) {
    auto *baseClass = c.get();
    if (!baseClass->isSubClassOf(inst)) {
      continue;
    }

    OS << "class " << name << ";\n";
    OS << "template<> ";
    OS << name << " *cast_or_null<" << name << ">";
    OS << "(Value *value);\n";

    OS << "template<> ";
    OS << "inline const " << name << " *cast_or_null<const " << name << ">";
    OS << "(const Value *value) {\n";
    OS << "\treturn ::cast_or_null<" << name << ">(const_cast<Value *>(value));\n";
    OS << "}\n";
  }
  OS << "#endif // GET_CAST_INTF\n\n";

  OS << "#ifdef GET_CAST_IMPL\n";
  OS << "#undef GET_CAST_IMPL\n";
  for (const auto &[name, c] : records_.getClasses()) {
    auto *baseClass = c.get();
    if (!baseClass->isSubClassOf(inst)) {
      continue;
    }

    OS << "template<> ";
    OS << name << " *cast_or_null<" << name << ">";
    OS << "(Value *value) {\n";
    OS << "\tauto *i = ::cast_or_null<Inst>(value); if (!i) return nullptr;\n";
    OS << "\tswitch (i->GetKind()) {\n\t\tdefault: return nullptr;\n";
    for (auto *r : records_.getAllDerivedDefinitions(name)) {
      OS << "\t\tcase Inst::Kind::" << r->getName() << ": \n";
    }
    OS << "\t\t\treturn reinterpret_cast<" << name << "*>(value);\n";
    OS << "\t}\n";
    OS << "}\n";
  }
  OS << "#endif // GET_CAST_IMPL\n\n";
}

