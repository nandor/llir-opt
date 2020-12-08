// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "get_bitcode.h"
#include "util.h"



// -----------------------------------------------------------------------------
void GetBitcodeWriter::run(llvm::raw_ostream &OS)
{
  OS << "#ifdef GET_BITCODE_READER\n";
  OS << "#undef GET_BITCODE_READER\n";
  GetReader(OS);
  OS << "#endif // GET_BITCODE_READER\n\n";
  OS << "#ifdef GET_BITCODE_WRITER\n";
  OS << "#undef GET_BITCODE_WRITER\n";
  GetWriter(OS);
  OS << "#endif // GET_BITCODE_WRITER\n\n";
}

// -----------------------------------------------------------------------------
void GetBitcodeWriter::GetReader(llvm::raw_ostream &OS)
{
  for (auto r : records_.getAllDerivedDefinitions("Inst")) {
    if (r->getValueAsBit("HasCustomReader")) {
      continue;
    }

    llvm::StringRef name(r->getName());
    auto type = GetTypeName(*r);
    OS << "case Inst::Kind::" << name << ": {\n";

    // Emit code to read types.
    int numTypes = r->getValueAsInt("NumTypes");
    if (numTypes < 0) {
      OS << "std::vector<Type> types;\n";
      OS << "for (unsigned i = 0, n = ReadData<uint8_t>(); i < n; ++i) ";
      OS << "types.push_back(ReadType());\n";
    } else {
      for (int i = 0; i < numTypes; ++i) {
        OS << "Type t" << i << " = static_cast<Type>(ReadData<uint8_t>());\n";
      }
    }

    // Emit code to read fields.
    auto fields = r->getValueAsListOfDefs("Fields");
    for (unsigned i = 0, n = fields.size(); i < n; ++i) {
      auto *field = fields[i];
      auto fieldType = field->getValueAsString("Type");
      if (field->getValueAsBit("IsList")) {
        if (field->getValueAsBit("IsScalar")) {
          llvm_unreachable("not implemented");
        } else {
          OS << "std::vector<Ref<" << fieldType << ">> arg" << i << ";";
          OS << "for (unsigned i = 0, n = ReadData<uint16_t>(); i < n; ++i)";
          OS << "arg" << i << ".push_back(Read" << fieldType << "(map));";
        }
      } else {
        if (field->getValueAsBit("IsScalar")) {
          OS << "using T" << i;
          OS << " = sized_uint<sizeof(" << fieldType << ")>::type;";
          if (field->getValueAsBit("IsOptional")) {
            OS << "std::optional<" << fieldType << "> arg" << i << ";";
            OS << "if (auto v = ReadData<T" << i << ">()) ";
            OS << "arg" << i << " = static_cast<" << fieldType << ">(v - 1);";
          } else {
            OS << "auto arg" << i << " = static_cast<" << fieldType << ">(";
            OS << "ReadData<T" << i << ">());";
          }
        } else {
          OS << "auto arg" << i << " = Read" << fieldType << "(map);";
        }
      }
      OS << "\n";
    }
    OS << "return new " << type << "Inst(";
    if (numTypes < 0) {
      OS << "types, ";
    } else {
      for (int i = 0; i < numTypes; ++i) {
        OS << "t" << i << ", ";
      }
    }
    for (unsigned i = 0, n = fields.size(); i < n; ++i) {
      OS << "arg" << i << ", ";
    }
    OS << "std::move(annots));\n}\n";
  }

}

// -----------------------------------------------------------------------------
void GetBitcodeWriter::GetWriter(llvm::raw_ostream &OS)
{
  for (auto r : records_.getAllDerivedDefinitions("Inst")) {
    if (r->getValueAsBit("HasCustomWriter")) {
      continue;
    }

    llvm::StringRef name(r->getName());
    auto type = GetTypeName(*r);
    OS << "case Inst::Kind::" << name << ": {\n";
    OS << "const auto &v = static_cast<const " << type << "Inst &>(i);\n";
    // Emit code to read types.
    int numTypes = r->getValueAsInt("NumTypes");
    if (numTypes < 0) {
      OS << "Emit<uint8_t>(v.type_size());\n";
      OS << "for (Type t : v.types()) Write(t); \n";
    } else {
      for (int i = 0; i < numTypes; ++i) {
        OS << "Write(i.GetType(" << i << "));\n";
      }
    }
    // Emit code to read fields.
    auto fields = r->getValueAsListOfDefs("Fields");
    for (unsigned i = 0, n = fields.size(); i < n; ++i) {
      OS << "{";
      auto *field = fields[i];
      auto fieldType = field->getValueAsString("Type");
      auto fieldName = field->getValueAsString("Name");
      if (field->getValueAsBit("IsList")) {
        if (field->getValueAsBit("IsScalar")) {
          llvm_unreachable("not implemented");
        } else {
          auto itName = llvm::StringRef(fieldName.lower()).drop_back();
          OS << "size_t n = v." << itName << "_size(); ";
          OS << "Emit<uint16_t>(n);";
          OS << "for (size_t i = 0; i < n; ++i)";
          OS << "Write" << fieldType << "(v." << itName << "(i), map);";
        }
      } else {
        if (field->getValueAsBit("IsScalar")) {
          OS << "using T = sized_uint<sizeof(" << fieldType << ")>::type;";
          if (field->getValueAsBit("IsOptional")) {
            OS << "if (auto op = v.Get" << fieldName << "()) {";
            OS << "Emit<T>(static_cast<T>(*op) + 1);";
            OS << "} else { Emit<T>(0); }";
          } else {
            OS << "Emit<T>(static_cast<T>(v.Get" << fieldName << "()));";
          }
        } else {
          OS << "Write" << fieldType << "(v.Get" << fieldName  << "(), map);";
        }
      }
      OS << "};\n";
    }


    OS << "return;};\n";
  }
}
