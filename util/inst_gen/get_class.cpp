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
    if (baseClass == inst || baseClass->getName() == "CallSite") {
      continue;
    }
    EmitClassIntf(OS, *baseClass);
  }
  OS << "#endif // GET_BASE_INTF\n\n";

  OS << "#ifdef GET_BASE_IMPL\n";
  OS << "#undef GET_BASE_IMPL\n";
  for (auto *baseClass : bases_) {
    if (baseClass == inst || baseClass->getName() == "CallSite") {
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
llvm::Record *GetBase(llvm::Record &r)
{
  auto bases = r.getType()->getClasses();
  assert(bases.size() == 1 && "single base expected");
  return bases[0];
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
    if (field->getValueAsBit("IsList")) {
      if (field->getValueAsBit("IsScalar")) {
        llvm_unreachable("not implemented");
      } else {
        llvm_unreachable("not implemented");
      }
    } else {
      if (field->getValueAsBit("IsScalar")) {
        if (field->getValueAsBit("IsOptional")) {
          OS << "std::optional<" << fieldType << ">";
        } else {
          OS << fieldType;
        }
      } else {
        OS << "Ref<" << fieldType << ">";
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
  for (auto *field : allFields) {
    if (!field->getValueAsBit("IsScalar")) {
      numRefFields++;
    }
  }
  OS << GetTypeName(r) << "::" << GetTypeName(r) << "(";
  EmitConsTypes(OS, r, fields, annot);
  OS << ")\n";
  OS << " : " << r.getType()->getAsString() << "(";
  if (!r.isClass()) {
    OS << "Kind::" << r.getName() << ",";
    OS << numRefFields << ", ";
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
  std::vector<std::pair<llvm::Record *, unsigned>> ownRefFields;
  unsigned refIndex = 0;
  for (unsigned i = 0, n = allFields.size(); i < n; ++i) {
    auto *field = allFields[i];
    auto fieldName = field->getValueAsString("Name");
    if (field->getValueAsBit("IsList")) {
      llvm_unreachable("not implemented");
    } else {
      if (field->getValueAsBit("IsScalar")) {
        if (ownFields.count(fieldName.str())) {
          OS << ", " << fieldName << "_(arg" << i << ")";
        }
      } else {
        if (ownFields.count(fieldName.str())) {
          ownRefFields.emplace_back(field, refIndex);
        }
        refIndex++;
      }
    }
  }
  OS << "{\n";
  for (auto [field, idx] : ownRefFields) {
    auto it = std::find(fields.begin(), fields.end(), field);
    OS << "Set<" << idx << ">";
    if (it == fields.end()) {
      OS << "(nullptr);";
    } else {
      OS << "(arg" << (it - fields.begin()) << ");\n";
    }
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
    OS << "~" << type << "();";
  }

  EmitAttr(OS, r, *base, "IsReturn");
  EmitAttr(OS, r, *base, "IsConstant");
  EmitAttr(OS, r, *base, "IsTerminator");
  EmitAttr(OS, r, *base, "HasSideEffects");

  for (auto *field : r.getValueAsListOfDefs("Fields")) {
    auto fieldName = field->getValueAsString("Name");
    auto fieldType = field->getValueAsString("Type");
    if (fields.count(fieldName.str()) == 0) {
      continue;
    }
    if (field->getValueAsBit("IsList")) {
      if (field->getValueAsBit("IsScalar")) {
        llvm_unreachable("not implemented");
      } else {
        llvm_unreachable("not implemented");
      }
    } else {
      if (field->getValueAsBit("IsScalar")) {
        OS << "private: ";
        if (field->getValueAsBit("IsOptional")) {
          OS << "std::optional<" << fieldType << ">";
        } else {
          OS << fieldType;
        }
        OS << " " << fieldName << "_;";
        OS << "public:\n";

        if (field->getValueAsBit("IsOptional")) {
          OS << "std::optional<" << fieldType << ">";
        } else {
          OS << fieldType;
        }
        OS << " Get" << fieldName << "() const ";
        OS <<" { return " << fieldName << "_; }\n";
      } else {
        OS << "Ref<" << fieldType << "> Get" << fieldName << "();\n";
        OS << "ConstRef<" << fieldType << "> Get" << fieldName << "() const;\n";
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
      OS << "private: std::vector<Type> types_; public: \n";
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
      OS << "virtual unsigned GetNumRets() const override{ return types_.size(); }\n";
    } else if (ntys >= btys) {
      OS << "virtual unsigned GetNumRets() const override{ return " << ntys << "; }\n";
      OS << "private:\n";
      for (int i = btys; i < ntys; ++i) {
        OS << "const Type t" << i << "_;\n";
      }
      OS << "public:\n";
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
  } else if (r.isSubClassOf(termBase)) {
    int nsucc = r.getValueAsInt("NumSuccessors");
    int bsucc = base->getValueAsInt("NumSuccessors");
    if (nsucc != bsucc) {
      llvm_unreachable("not implemented");
    }
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

  unsigned idx = 0;
  for (auto *field : r.getValueAsListOfDefs("Fields")) {
    auto fieldName = field->getValueAsString("Name");
    auto fieldType = field->getValueAsString("Type");
    if (fields.count(fieldName.str()) == 0) {
      continue;
    }
    if (field->getValueAsBit("IsList")) {
      if (field->getValueAsBit("IsScalar")) {
        llvm_unreachable("not implemented");
      } else {
        llvm_unreachable("not implemented");
      }
    } else {
      if (!field->getValueAsBit("IsScalar")) {
        OS << "Ref<" << fieldType << "> " << type;
        OS << "::Get" << fieldName << "() {";
        if (field->getValueAsBit("IsOptional")) {
          OS << " return ::cast_or_null<" << fieldType << ">(Get<" << idx << ">()); }\n";
        } else {
          OS << " return ::cast<" << fieldType << ">(Get<" << idx << ">()); }\n";
        }

        OS << "ConstRef<" << fieldType << "> " << type;
        OS << "::Get" << fieldName << "() const {";
        if (field->getValueAsBit("IsOptional")) {
          OS << " return ::cast_or_null<" << fieldType << ">(Get<" << idx << ">()); }\n";
        } else {
          OS << " return ::cast<" << fieldType << ">(Get<" << idx << ">()); }\n";
        }
        ++idx;
      }
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
}
