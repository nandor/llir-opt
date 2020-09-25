// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/Support/ErrorHandling.h>

#include "core/annot.h"



// -----------------------------------------------------------------------------
AnnotSet::AnnotSet()
{
}

// -----------------------------------------------------------------------------
AnnotSet::AnnotSet(AnnotSet &&that)
  : annots_(std::move(that.annots_))
{
}

// -----------------------------------------------------------------------------
AnnotSet::AnnotSet(const AnnotSet &that)
{
  for (const Annot &annot : that) {
    switch (annot.GetKind()) {
      case Annot::Kind::CAML_FRAME: {
        Set<CamlFrame>(static_cast<const CamlFrame &>(annot));
        continue;
      }
      case Annot::Kind::CAML_ADDR: {
        Set<CamlAddr>(static_cast<const CamlAddr &>(annot));
        continue;
      }
      case Annot::Kind::CAML_VALUE: {
        Set<CamlValue>(static_cast<const CamlValue &>(annot));
        continue;
      }
    }
    llvm_unreachable("invalid annotation kind");
  }
}

// -----------------------------------------------------------------------------
AnnotSet::~AnnotSet()
{
}

// -----------------------------------------------------------------------------
bool AnnotSet::Add(const Annot &annot)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
bool AnnotSet::operator==(const AnnotSet &that) const
{
  for (const Annot &thisAnnot : annots_) {
    bool found = false;
    for (const Annot &thatAnnot : that.annots_) {
      if (thisAnnot.GetKind() != thatAnnot.GetKind()) {
        continue;
      }
      llvm_unreachable("not implemented");
    }
    if (!found) {
      return false;
    }
  }
  for (const Annot &thatAnnot : that.annots_) {
    bool found = false;
    for (const Annot &thisAnnot : annots_) {
      if (thisAnnot.GetKind() != thatAnnot.GetKind()) {
        continue;
      }
      llvm_unreachable("not implemented");
    }
    if (!found) {
      return false;
    }
  }
  return true;
}

// -----------------------------------------------------------------------------
AnnotSet &AnnotSet::operator=(AnnotSet &&that)
{
  annots_.splice(annots_.end(), that.annots_);
  return *this;
}

// -----------------------------------------------------------------------------
CamlFrame::CamlFrame(
    std::vector<size_t> &&allocs,
    bool raise,
    std::vector<DebugInfos> &&debug_infos)
  : Annot(Kind::CAML_FRAME)
  , allocs_(std::move(allocs))
  , raise_(raise)
  , debug_infos_(std::move(debug_infos))
{
}
