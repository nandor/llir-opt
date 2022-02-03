// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "get_parser.h"
#include "util.h"



// -----------------------------------------------------------------------------
void GetParserWriter::run(llvm::raw_ostream &OS)
{
  OS << "#ifdef GET_PARSER\n";

  RecordList records;
  for (auto r : records_.getAllDerivedDefinitions("Inst")) {
    if (r->getValueAsBit("HasCustomParser")) {
      continue;
    }
    records.push_back(r);
  }

  std::sort(records.begin(), records.end(), [] (auto *a, auto *b) {
    return a->getName() < b->getName();
  });

  PrintTrie(OS, 0, records.begin(), records.end());

  OS << "#undef GET_PARSER\n";
  OS << "#endif // GET_PARSER\n";
}

// -----------------------------------------------------------------------------
void GetParserWriter::PrintTrie(
    llvm::raw_ostream &OS,
    unsigned index,
    RecordList::iterator begin,
    RecordList::iterator end)
{
  assert(end != begin && "empty set of records");

  OS << "if (opc.size() <= " << index << ") {\n";
  if (index >= (*begin)->getName().size()) {
    PrintParser(OS, *begin);
    ++begin;
  }
  OS << "} else { switch (opc[" << index << "]) {\n";

  while (begin != end) {
    char chr = (*begin)->getName().lower()[index];

    OS << "case '" << chr << "': {\n";

    auto mid = begin;
    while ((*mid)->getName().lower()[index] == chr && mid != end) {
      ++mid;
    }
    PrintTrie(OS, index + 1, begin, mid);
    begin = mid;
    OS << "break;}\n";
  }

  OS << "}}\n";
}

// -----------------------------------------------------------------------------
void GetParserWriter::PrintParser(llvm::raw_ostream &OS, llvm::Record *r)
{
  OS << "// " << r->getName().lower() << "\n";

  OS << "return new " << GetTypeName(*r) << "(";

  int ntys = r->getValueAsInt("NumTypes");
  if (ntys < 0) {
    llvm_unreachable(r->getName().data());
  } else {
    for (unsigned i = 0; i < ntys; ++i) {
      OS << "t(" << i << "), ";
    }
  }

  auto fields = r->getValueAsListOfDefs("Fields");
  for (unsigned i = 0, n = fields.size(); i < n; ++i) {
    auto *field = fields[i];
    auto fieldType = field->getValueAsString("Type");
    auto fieldName = field->getValueAsString("Name");
    if (field->getValueAsBit("IsList")) {
      llvm_unreachable(r->getName().data());
    } else {
      if (field->getValueAsBit("IsScalar")) {
        if (field->getValueAsBit("IsOptional")) {
          llvm_unreachable(r->getName().data());
        } else {
          OS << "Op" << fieldType << "(" << ntys + i << "),";
        }
      } else {
        if (field->getValueAsBit("IsOptional")) {
          llvm_unreachable(r->getName().data());
        } else {
          OS << "Op" << fieldType << "(" << ntys + i << "),";
        }
      }
    }
  }

  OS << "std::move(annot));\n";
}
