// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <set>

#include "get_class.h"
#include "util.h"


// -----------------------------------------------------------------------------
static void Topo(llvm::Record *r, std::vector<llvm::Record *> &sorted)
{
  if (std::find(sorted.begin(), sorted.end(), r) != sorted.end()) {
    return;
  }
  for (auto [super, loc] : r->getSuperClasses()) {
    Topo(super, sorted);
  }
  sorted.push_back(r);
}

// -----------------------------------------------------------------------------
void GetClassWriter::run(llvm::raw_ostream &OS)
{
  auto *inst = records_.getClass("Inst");
  for (const auto &[name, c] : records_.getClasses()) {
    auto *baseClass = c.get();
    if (!baseClass->isSubClassOf(inst)) {
      continue;
    }
    Topo(baseClass, bases_);
  }

  OS << "#ifdef GET_BASE_INTF\n";
  OS << "#undef GET_BASE_INTF\n";
  for (auto *baseClass : bases_) {
    if (baseClass == inst) {
      continue;
    }
    EmitClassIntf(OS, *baseClass);
  }
  OS << "#endif // GET_BASE_INTF\n\n";

  OS << "#ifdef GET_BASE_IMPL\n";
  OS << "#undef GET_BASE_IMPL\n";
  for (auto *baseClass : bases_) {
    if (baseClass == inst) {
      continue;
    }
    EmitClassImpl(OS, *baseClass);
  }
  OS << "#endif // GET_BASE_IMPL\n\n";

  OS << "#ifdef GET_CLASS_INTF\n";
  OS << "#undef GET_CLASS_INTF\n";
  for (auto *r : records_.getAllDerivedDefinitions("Inst")) {
    if (r->getValueAsBit("HasCustomDefinition")) {
      continue;
    }
    EmitClassIntf(OS, *r);
  }
  OS << "#endif // GET_CLASS_INTF\n\n";

  OS << "#ifdef GET_CLASS_IMPL\n";
  OS << "#undef GET_CLASS_IMPL\n";
  for (auto *r : records_.getAllDerivedDefinitions("Inst")) {
    if (r->getValueAsBit("HasCustomDefinition")) {
      continue;
    }
    EmitClassImpl(OS, *r);
  }
  OS << "#endif // GET_CLASS_IMPL\n\n";
}

// -----------------------------------------------------------------------------
std::set<std::string> GetOwnFields(llvm::Record &r)
{
  std::set<std::string> fields;
  for (auto *field : r.getValueAsListOfDefs("Fields")) {
    fields.insert(field->getValueAsString("Name").str());
  }
  for (auto *field : GetBase(r)->getValueAsListOfDefs("Fields")) {
    fields.erase(field->getValueAsString("Name").str());
  }
  return fields;
}

// -----------------------------------------------------------------------------
typedef void (*ConsEmitter) (
    llvm::raw_ostream &OS,
    llvm::Record &r,
    llvm::ArrayRef<llvm::Record *> fields,
    bool annot
);

// -----------------------------------------------------------------------------
static void EmitConsTypes(
    llvm::raw_ostream &OS,
    llvm::Record &r,
    llvm::ArrayRef<llvm::Record *> fields,
    bool annot)
{
  if (r.isClass()) {
    OS << "Kind kind, unsigned nops, ";
  }
  int numTypes = r.getValueAsInt("NumTypes");
  if (numTypes < 0) {
    OS << "llvm::ArrayRef<Type> types,";
  } else {
    for (int i = 0; i < numTypes; ++i) {
      OS << "Type t" << i << ",";
    }
  }
  for (unsigned i = 0, n = fields.size(); i < n; ++i) {
    auto *field = fields[i];
    auto fieldType = field->getValueAsString("Type");

    const bool isScalar = field->getValueAsBit("IsScalar");
    const bool isOptional = field->getValueAsBit("IsOptional");
    const bool isUnwrapped = field->getValueAsBit("IsUnwrapped");

    if (field->getValueAsBit("IsList")) {
      OS << "llvm::ArrayRef<";
      if (isScalar) {
         OS << fieldType;
      } else {
        if (isUnwrapped) {
          OS << fieldType << "*";
        } else {
          OS << "Ref<" << fieldType << ">";
        }
      }
      OS << ">";
    } else {
      if (isScalar) {
        if (isOptional) {
          OS << "std::optional<" << fieldType << ">";
        } else {
          OS << fieldType;
        }
      } else {
        if (isUnwrapped) {
          OS << fieldType << " *";
        } else {
          OS << "Ref<" << fieldType << ">";
        }
      }
    }
    OS << " arg" << i << ",";
  }

  if (annot) {
    OS << "const AnnotSet &annot";
  } else {
    OS << "AnnotSet &&annot";
  }
}

// -----------------------------------------------------------------------------
static void EmitConsIntf(
    llvm::raw_ostream &OS,
    llvm::Record &r,
    llvm::ArrayRef<llvm::Record *> fields,
    bool annot)
{
  OS << GetTypeName(r) << "(";
  EmitConsTypes(OS, r, fields, annot);
  OS << ");\n";
}

// -----------------------------------------------------------------------------
static void EmitConsImpl(
    llvm::raw_ostream &OS,
    llvm::Record &r,
    llvm::ArrayRef<llvm::Record *> fields,
    bool annot)
{
  auto *base = GetBase(r);
  auto ownFields = GetOwnFields(r);
  auto allFields = r.getValueAsListOfDefs("Fields");

  int ntys = r.getValueAsInt("NumTypes");
  int btys = base->getValueAsInt("NumTypes");
  assert(
      ((ntys <= 0 && btys <= 0) || (ntys >= 0 && btys >= 0)) &&
      "invalid type description"
  );

  unsigned numRefFields = 0;
  std::string sumRefFields;
  llvm::raw_string_ostream os(sumRefFields);
  for (unsigned i = 0, n = allFields.size(); i < n; ++i) {
    auto *field = allFields[i];
    auto fieldName = field->getValueAsString("Name");
    if (!field->getValueAsBit("IsScalar")) {
      if (field->getValueAsBit("IsList")) {
        if (!sumRefFields.empty()) {
          os << " + ";
        }
        os << "arg" << i << ".size()";
      } else {
        numRefFields++;
      }
    }
  }
  OS << GetTypeName(r) << "::" << GetTypeName(r) << "(";
  EmitConsTypes(OS, r, fields, annot);
  OS << ")\n";
  OS << " : " << r.getType()->getAsString() << "(";
  if (!r.isClass()) {
    OS << "Kind::" << r.getName() << ",";
    OS << sumRefFields << " + " << numRefFields << ", ";
  } else {
    OS << "kind, nops,";
  }
  if (ntys < 0 && btys < 0) {
    OS << "types,";
  } else if (ntys >= btys) {
    for (int i = 0; i < btys; ++i) {
      OS << "t" << i << ",";
    }
  }
  for (unsigned i = 0, n = fields.size(); i < n; ++i) {
    auto *field = fields[i];
    auto fieldName = field->getValueAsString("Name");
    if (!ownFields.count(fieldName.str())) {
      OS << "arg" << i << ",";
    }
  }
  OS << (annot ? "annot" : "std::move(annot)");
  OS << ")";
  if (ntys < 0 && btys >= 0) {
    OS << ", types_(types)";
  } else if (ntys >= btys) {
    for (int i = btys; i < ntys; ++i) {
      OS << ", t" << i << "_(t" << i << ")";
    }
  }

  for (unsigned i = 0, n = allFields.size(); i < n; ++i) {
    auto *field = allFields[i];
    auto fieldName = field->getValueAsString("Name").str();
    if (ownFields.count(fieldName)) {
      if (field->getValueAsBit("IsScalar")) {
        OS << ", " << fieldName << "_(arg" << i << ")";
      } else {
        if (field->getValueAsBit("IsList")) {
          OS << ", num" << fieldName << "_(arg" << i << ".size())";
        }
      }
    }
  }

  OS << "{\n";

  std::vector<std::pair<llvm::StringRef, bool>> preds;
  unsigned lastOffset = 0;
  for (auto *field : allFields) {
    if (field->getValueAsBit("IsScalar")) {
      continue;
    }

    OS << "{";

    auto fieldName = field->getValueAsString("Name");
    const bool isList = field->getValueAsBit("IsList");
    const bool isOwnField = ownFields.count(fieldName.str());

    // Find the index the argument is at.
    OS << "size_t base = 0";
    for (auto &[name, arr] : preds) {
      OS << "+";
      if (arr) {
        OS << "num" << name << "_";
      } else {
        OS << "1";
      }
    }
    OS << ";";

    auto it = std::find(fields.begin(), fields.end(), field);
    if (isList) {
      OS << "for (unsigned i = 0; i < num" << fieldName << "_; ++i) ";
      OS << "Set(base + i, arg" << (it - fields.begin()) << "[i]);";
    } else {
      if (it == fields.end()) {
        OS << "Set(base, nullptr);";
      } else {
        OS << "Set(base, arg" << (it - fields.begin()) << ");";
      }
    }
    preds.emplace_back(fieldName, isList);

    OS << "};\n";
  }

  OS << "};\n";
}

// -----------------------------------------------------------------------------
static void EmitConsVariant(
    llvm::raw_ostream &OS,
    llvm::Record &r,
    std::vector<llvm::Record *> &fields,
    unsigned i,
    bool annot,
    ConsEmitter Fn)
{
  auto allFields = r.getValueAsListOfDefs("Fields");
  if (i == allFields.size()) {
    return (*Fn)(OS, r, fields, annot);
  }

  auto *field = allFields[i];
  if (field->getValueAsBit("IsOptional") && !field->getValueAsBit("IsScalar")) {
    EmitConsVariant(OS, r, fields, i + 1, annot, Fn);
  }

  fields.push_back(field);
  EmitConsVariant(OS, r, fields, i + 1, annot, Fn);
  fields.pop_back();
}

// -----------------------------------------------------------------------------
static void EmitCons(
    llvm::raw_ostream &OS,
    llvm::Record &r,
    bool annot,
    ConsEmitter Fn)
{
  std::vector<llvm::Record *> fields;
  EmitConsVariant(OS, r, fields, 0, annot, Fn);
}

// -----------------------------------------------------------------------------
static void EmitAttr(
    llvm::raw_ostream &OS,
    llvm::Record &r,
    llvm::Record &b,
    const char *Name)
{
  bool flag = r.getValueAsBit(Name);
  if (b.getValueAsBit(Name) != flag) {
    OS << "bool " << Name << "() const override { return " << flag << "; }\n";
  }
}

// -----------------------------------------------------------------------------
void GetClassWriter::EmitClassIntf(llvm::raw_ostream &OS, llvm::Record &r)
{
  auto type = GetTypeName(r);
  auto base = GetBase(r);
  auto fields = GetOwnFields(r);

  // Declare the class.
  OS << "class " << type << " : public " << base->getName() << " {\n";
  OS << "public:\n";
  if (!r.isClass()) {
    OS << "static constexpr Kind kInstKind = Kind::" << r.getName() << ";\n";
  }

  EmitCons(OS, r, false, EmitConsIntf);
  EmitCons(OS, r, true, EmitConsIntf);
  if (r.isClass()) {
    OS << "virtual ~" << type << "();";
  }

  EmitAttr(OS, r, *base, "IsReturn");
  EmitAttr(OS, r, *base, "IsConstant");
  EmitAttr(OS, r, *base, "IsTerminator");
  EmitAttr(OS, r, *base, "HasSideEffects");

  std::vector<std::pair<llvm::StringRef, int>> blocks;
  std::vector<std::string> lastName;
  unsigned lastOffset = 0, i = 0;
  for (auto *field : r.getValueAsListOfDefs("Fields")) {
    auto fieldName = field->getValueAsString("Name");
    auto fieldType = field->getValueAsString("Type");

    const bool isList = field->getValueAsBit("IsList");
    const bool isScalar = field->getValueAsBit("IsScalar");
    const bool isUnwrapped = field->getValueAsBit("IsUnwrapped");
    const bool isOptional = field->getValueAsBit("IsOptional");

    const bool isOwnField = fields.count(fieldName.str()) != 0;

    if (isScalar) {
      if (isOwnField) {
        if (isList) {
          auto itName = llvm::StringRef(fieldName.lower()).drop_back().str();
          OS << "protected: std::vector<" << fieldType << "> " << fieldName << "_; public:\n";

          OS << "llvm::ArrayRef<" << fieldType << "> Get" << fieldName << "() const";
          OS << "{ return " << fieldName << "_; }\n";

          OS << fieldType << " " << itName << "(unsigned idx) const";
          OS << "{ return " << fieldName << "_[idx]; }\n";
        } else {
          OS << "protected: ";
          if (isOptional) {
            OS << "std::optional<" << fieldType << ">";
          } else {
            OS << fieldType;
          }
          OS << " " << fieldName << "_;";
          OS << "public:\n";

          if (isOptional) {
            OS << "std::optional<" << fieldType << ">";
          } else {
            OS << fieldType;
          }
          OS << " Get" << fieldName << "() const ";
          OS <<" { return " << fieldName << "_; }\n";
        }
      }
    } else {
      if (isList) {
        if (isOwnField) {
          auto itName = llvm::StringRef(fieldName.lower()).drop_back().str();
          OS << "protected: size_t num" << fieldName << "_;public:\n";

          OS << "size_t " << itName << "_size() const { return num" << fieldName << "_; }\n";
          OS << "bool " << itName << "_empty() const { return 0 == " << itName << "_size(); }\n";

          if (isUnwrapped) {
            OS << "using " << itName << "_iterator = unref_iterator<" << fieldType << ">;\n";
            OS << "using " << itName << "_range = unref_range<" << fieldType << ">;\n";
            OS << "using const_" << itName << "_iterator = const_unref_iterator<" << fieldType << ">;\n";
            OS << "using const_" << itName << "_range = const_unref_range<" << fieldType << ">;\n";

            OS << "const " << fieldType << " *" << itName << "(unsigned i) const;\n";
            OS << fieldType << " *" << itName << "(unsigned i);\n";
          } else {
            OS << "using " << itName << "_iterator = conv_op_iterator<" << fieldType << ">;\n";
            OS << "using " << itName << "_range = conv_op_range<" << fieldType << ">;\n";
            OS << "using const_" << itName << "_iterator = const_conv_op_iterator<" << fieldType << ">;\n";
            OS << "using const_" << itName << "_range = const_conv_op_range<" << fieldType << ">;\n";

            OS << "ConstRef<" << fieldType << "> " << itName << "(unsigned i) const;\n";
            OS << "Ref<" << fieldType << "> " << itName << "(unsigned i);\n";
          }

          auto base = [&, this]
          {
            bool first = true;
            for (auto &elem : lastName) {
              if (!first) {
                OS << " + ";
              } else {
                first = false;
              }
              OS << "num" << elem << "_";
            }
            if (!first) {
              OS << " + ";
            }
            OS << lastOffset;
          };

          OS << itName << "_iterator " << itName << "_begin() ";
          OS << "{ return " << itName << "_iterator(this->value_op_begin() + ";
          base();
          OS << "); }\n";

          OS << itName << "_iterator " << itName << "_end() ";
          OS << "{ return " << itName << "_iterator(this->value_op_begin() + ";
          base();
          OS << " + num" << fieldName << "_); }\n";

          OS << itName << "_range " << itName << "s() ";
          OS <<" { return llvm::make_range";
          OS << "(" << itName << "_begin(), " << itName << "_end()); }\n";

          OS << "const_" << itName << "_iterator " << itName << "_begin() const ";
          OS << "{ return const_" << itName << "_iterator(this->value_op_begin() + ";
          base();
          OS << "); }\n";

          OS << "const_" << itName << "_iterator " << itName << "_end() const ";
          OS << "{ return const_" << itName << "_iterator(this->value_op_begin() + ";
          base();
          OS << " + num" << fieldName << "_); }\n";

          OS << "const_" << itName << "_range " << itName << "s() const ";
          OS <<" { return llvm::make_range";
          OS << "(" << itName << "_begin(), " << itName << "_end()); }\n";

          if (fieldType == "Block") {
            blocks.emplace_back(fieldName, -1);
          }
        }
        lastName.push_back(fieldName.str());
        lastOffset = 0;
      } else {
        if (isOwnField) {
          if (isUnwrapped) {
            OS << fieldType << "* ";
          } else {
            OS << "Ref<" << fieldType << "> ";
          }
          OS << "Get" << fieldName << "();\n";

          if (isUnwrapped) {
            OS << "const " << fieldType << "* ";
          } else {
            OS << "ConstRef<" << fieldType << "> ";
          }
          OS << "Get" << fieldName << "() const;\n";

          if (isUnwrapped && fieldType == "Block") {
            blocks.emplace_back(fieldName, i);
          }
        }
        i++;
        lastOffset += 1;
      }
    }
  }

  int ntys = r.getValueAsInt("NumTypes");
  int btys = base->getValueAsInt("NumTypes");
  if (ntys != btys) {
    OS << "virtual Type GetType(unsigned i) const override;\n";
    if (ntys == 1) {
      OS << "Type GetType() const { return GetType(0); }\n";
    }

    if (ntys < 0 && btys >= 0) {
      OS << "protected: std::vector<Type> types_; public: \n";
      OS << "using type_iterator = std::vector<Type>::iterator;\n";
      OS << "using const_type_iterator = std::vector<Type>::const_iterator;\n";
      OS << "using type_range = llvm::iterator_range<type_iterator>;\n";
      OS << "using const_type_range = llvm::iterator_range<const_type_iterator>;\n";
      OS << "size_t type_size() const { return types_.size(); }\n";
      OS << "bool type_empty() const { return types_.empty(); }\n";
      OS << "Type type(unsigned i) const { return types_[i]; }\n";
      OS << "type_iterator type_begin() { return types_.begin(); }\n";
      OS << "const_type_iterator type_begin() const { return types_.begin(); }\n";
      OS << "type_iterator type_end() { return types_.end(); }\n";
      OS << "const_type_iterator type_end() const { return types_.end(); }\n";
      OS << "type_range types() { return llvm::make_range(type_begin(), type_end()); }\n";
      OS << "const_type_range types() const { return llvm::make_range(type_begin(), type_end()); }\n";
      OS << "llvm::ArrayRef<Type> GetTypes() const { return types_; }\n";
    } else if (ntys >= btys) {
      OS << "protected:\n";
      for (int i = btys; i < ntys; ++i) {
        OS << "const Type t" << i << "_;\n";
      }
      OS << "public:\n";
    }
  }

  int nrets = r.getValueAsInt("NumReturns");
  int brets = base->getValueAsInt("NumReturns");
  if (nrets != brets && nrets != -1) {
    OS << "virtual unsigned GetNumRets() const override { return " << nrets << "; }\n";
  } else {
    if (ntys != btys) {
      if (ntys < 0 && btys >= 0) {
        OS << "virtual unsigned GetNumRets() const override { return types_.size(); }\n";
      } else {
        OS << "virtual unsigned GetNumRets() const override { return " << ntys << "; }\n";
      }
    }
  }

  auto *termBase = records_.getClass("TerminatorInst");
  if (termBase == &r) {
    OS << "virtual unsigned getNumSuccessors() const ";
    OS << "{ return 0; }\n";
    OS << "virtual const Block *getSuccessor(unsigned idx) const ";
    OS << "{ llvm_unreachable(\"invalid successor\"); }\n";
    OS << "virtual Block *getSuccessor(unsigned idx) ";
    OS << "{ llvm_unreachable(\"invalid successor\"); }\n";
  } else if (r.isSubClassOf(termBase) && !blocks.empty()) {
    OS << "virtual unsigned getNumSuccessors() const override { return 0";

    for (auto &[name, idx] : blocks) {
      OS << " + ";
      if (idx >= 0) {
        OS << "1";
      } else {
        auto itName = llvm::StringRef(name.lower()).drop_back().str();
        OS << itName + "_size()";
      }
    }
    OS << "; }\n";

    OS << "virtual const Block *getSuccessor(unsigned idx) const override;\n";
    OS << "virtual Block *getSuccessor(unsigned idx) override;\n";
  }

  auto customIntf = r.getValueAsString("CustomIntf");
  auto baseCustomIntf = base->getValueAsString("CustomIntf");
  if (!customIntf.empty() && customIntf != baseCustomIntf) {
    OS << customIntf;
  }

  OS << "};\n";
}

// -----------------------------------------------------------------------------
void GetClassWriter::EmitClassImpl(llvm::raw_ostream &OS, llvm::Record &r)
{
  auto type = GetTypeName(r);
  auto base = GetBase(r);
  auto fields = GetOwnFields(r);

  EmitCons(OS, r, false, EmitConsImpl);
  EmitCons(OS, r, true, EmitConsImpl);
  if (r.isClass()) {
    OS << type << "::" <<  "~" << type << "() {}\n";
  }

  std::vector<std::pair<llvm::StringRef, bool>> preds;
  std::vector<std::pair<llvm::StringRef, bool>> blocks;
  size_t lastOffset = 0;
  for (auto *field : r.getValueAsListOfDefs("Fields")) {
    auto fieldName = field->getValueAsString("Name");
    auto fieldType = field->getValueAsString("Type");
    auto itName = llvm::StringRef(fieldName.lower()).drop_back().str();

    const bool isList = field->getValueAsBit("IsList");
    const bool isScalar = field->getValueAsBit("IsScalar");
    const bool isUnwrapped = field->getValueAsBit("IsUnwrapped");
    const bool isOptional = field->getValueAsBit("IsOptional");

    const bool isOwnField = fields.count(fieldName.str()) != 0;

    if (isScalar) {
      // Implemented in interface
    } else {
      auto idx = [&, this]
      {
        OS << "0";
        for (auto &[name, arr] : preds) {
          OS << " + ";
          if (arr) {
            OS << "num" << name << "_";
          } else {
            OS << "1";
          }
        }
      };

      if (isOwnField) {
        if (isList) {
          if (isUnwrapped) {
            OS << "const " << fieldType << " *";
          } else {
            OS << "ConstRef<" << fieldType << "> ";
          }

          OS << type << "::" << itName << "(unsigned i) const";
          OS << "{ return cast<" << fieldType << ">(static_cast<ConstRef<Value>>(Get(";
          idx();
          OS << " + i)))";
          if (isUnwrapped) {
            OS << ".Get()";
          }
          OS << "; }\n";

          if (isUnwrapped) {
            OS << fieldType << " *";
          } else {
            OS << "Ref<" << fieldType << "> ";
          }

          OS << type << "::" << itName << "(unsigned i) ";
          OS << "{ return cast<" << fieldType << ">(static_cast<Ref<Value>>(Get(";
          idx();
          OS << " + i)))";
          if (isUnwrapped) {
            OS << ".Get()";
          }
          OS << "; }\n";
        } else {
          if (isUnwrapped) {
            OS << fieldType << "* ";
          } else {
            OS << "Ref<" << fieldType << "> ";
          }
          OS << type << "::Get" << fieldName << "() { return ";
          if (isOptional) {
            OS << "::cast_or_null<" << fieldType << ">(";
          } else {
            OS << "::cast<" << fieldType << ">(";
          }
          OS << "Get("; idx(); OS << "))";
          if (isUnwrapped) {
            OS << ".Get()";
          }
          OS << "; }\n";

          if (isUnwrapped) {
            OS << "const " << fieldType << "* ";
          } else {
            OS << "ConstRef<" << fieldType << "> ";
          }
          OS << type << "::Get" << fieldName << "() const { return ";
          if (isOptional) {
            OS << "::cast_or_null<" << fieldType << ">(";
          } else {
            OS << "::cast<" << fieldType << ">(";
          }
          OS << "Get("; idx(); OS << "))";
          if (isUnwrapped) {
            OS << ".Get()";
          }
          OS << "; }\n";
        }

        if (isUnwrapped && fieldType == "Block") {
          blocks.emplace_back(fieldName, isList);
        }
      }

      preds.emplace_back(fieldName, isList);
    }
  }

  int ntys = r.getValueAsInt("NumTypes");
  int btys = base->getValueAsInt("NumTypes");
  if (ntys != btys) {
    OS << "Type " << type << "::GetType(unsigned i) const {\n";
    if (ntys < 0 && btys >= 0) {
      OS << "return types_[i];\n";
    } else {
      if (btys != 0) {
        llvm_unreachable("not implemented");
      }
      for (unsigned i = btys; i < ntys; ++i) {
        OS << "if (i == " << i << ") return t" << i << "_;\n";
      }
      OS << "llvm_unreachable(\"invalid type index\");\n";
    }
    OS << "}\n";
  }
  auto *termBase = records_.getClass("TerminatorInst");
  if (termBase != &r && r.isSubClassOf(termBase) && !blocks.empty()) {
    OS << "const Block *" << type << "::getSuccessor(unsigned idx) const ";
    OS << "{ return const_cast<" << type << " *>(this)->getSuccessor(idx); }\n";

    OS << "Block *" << type << "::getSuccessor(unsigned idx) {";

    bool allFixed = true;
    for (auto &[name, arr] : blocks) {
      if (arr) {
        allFixed = false;
        break;
      }
    }

    if (allFixed) {
      OS << "switch (idx) {";
      OS << "default: llvm_unreachable(\"invalid successor\");\n";
      for (unsigned i = 0, n = blocks.size(); i < n; ++i) {
        OS << "case " << i << ": return Get" << blocks[i].first << "();\n";
      }
      OS << "}";
    } else {
      if (blocks.size() == 1) {
        auto name = llvm::StringRef(blocks[0].first.lower()).drop_back().str();
        OS << "return " << name << "(idx);";
      } else {
        llvm_unreachable("not implemented");
      }
    }
    OS << "}\n";
  }
}
