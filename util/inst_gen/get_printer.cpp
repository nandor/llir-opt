// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "get_printer.h"
#include "util.h"



// -----------------------------------------------------------------------------
void GetPrinterWriter::run(llvm::raw_ostream &OS)
{
  OS << "#ifdef GET_PRINTER\n";
  for (auto r : records_.getAllDerivedDefinitions("Inst")) {
    if (r->getValueAsBit("HasCustomPrinter")) {
      continue;
    }

    auto name = r->getName();
    auto type = GetTypeName(*r);
    auto ntys = r->getValueAsInt("NumTypes");

    OS << "case Inst::Kind::" << name << ": {\n";
    OS << "auto &v = static_cast<const " << type << "&>(i);\n";
    OS << "os_ << \"" << name.lower() << "\\t\";";
    OS << "bool comma = false;";
    auto comma = [&OS] { OS << "os_ << (comma ? \", \" : \" \");"; };
    if (ntys < 0) {
      OS << "for (unsigned r = 0, n = i.GetNumRets(); r < n; ++r) {";
      comma();
      OS << "os_ << v.GetType(r) << \":\";";
      OS << "Print(v.GetSubValue(r));";
      OS << "comma=true;";
      OS << "}\n";
    } else {
      for (unsigned i = 0; i < ntys; ++i) {
      comma();
        OS << "os_ << v.GetType(" << i << ") << \":\";";
        OS << "Print(v.GetSubValue(" << i << "));";
        OS << "comma=true;\n";
      }
    }

    auto fields = r->getValueAsListOfDefs("Fields");
    for (unsigned i = 0, n = fields.size(); i < n; ++i) {
      auto *field = fields[i];
      auto fieldType = field->getValueAsString("Type");
      auto fieldName = field->getValueAsString("Name");
      if (field->getValueAsBit("IsList")) {
        if (field->getValueAsBit("IsScalar")) {
          OS << "for (auto arg : v.Get" << fieldName << "())";
          OS << "{"; comma(); OS << " os_ << arg; comma = true; };";
        } else {
          OS << "for (auto arg : v." << fieldName.lower() << "())";
          OS << "{"; comma(); OS << " Print(arg); comma = true; };";
        }
      } else {
        if (field->getValueAsBit("IsScalar")) {
          if (field->getValueAsBit("IsOptional")) {
            OS << "if (auto op = v.Get" << fieldName << "())";
            OS << "{"; comma(); OS << " os_ << *op; comma = true; }";
          } else {
            comma(); OS << "os_ << v.Get" << fieldName << "(); comma=true;";
          }
        } else {
          if (field->getValueAsBit("IsOptional")) {
            OS << "if (auto op = v.Get" << fieldName << "())";
            OS << "{";
            comma();
            OS << "Print(v.Get" << fieldName  << "()); comma=true;";
            OS << "}";
          } else {
            comma(); OS << "Print(v.Get" << fieldName  << "()); comma=true;";
          }
        }
      }
      OS << "\n";
    }
    OS << "return;}\n";
  }
  OS << "#undef GET_PRINTER\n";
  OS << "#endif // GET_PRINTER\n";
}
