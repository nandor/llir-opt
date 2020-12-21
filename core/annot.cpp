// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/Support/ErrorHandling.h>

#include "core/annot.h"



// -----------------------------------------------------------------------------
bool Annot::operator==(const Annot &that) const
{
  if (kind_ != that.kind_) {
    return false;
  }

  switch (kind_) {
    case Kind::CAML_FRAME: {
      return static_cast<const CamlFrame &>(*this) ==
             static_cast<const CamlFrame &>(that);
    }
    case Kind::PROBABILITY: {
      return static_cast<const Probability &>(*this) ==
             static_cast<const Probability &>(that);
    }
  }
  llvm_unreachable("invalid annotation kind");
}

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
      case Annot::Kind::PROBABILITY: {
        Set<Probability>(static_cast<const Probability &>(annot));
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
bool AnnotSet::Add(const Annot &newAnnot)
{
  switch (newAnnot.GetKind()) {
    case Annot::Kind::CAML_FRAME: {
      llvm_unreachable("not implemented");
    }
    case Annot::Kind::PROBABILITY: {
      llvm_unreachable("not implemented");
    }
  }
  llvm_unreachable("invalid annotation kind");
}

// -----------------------------------------------------------------------------
bool AnnotSet::operator==(const AnnotSet &that) const
{
  for (const Annot &thisAnnot : annots_) {
    bool found = false;
    for (const Annot &thatAnnot : that.annots_) {
      if (thisAnnot == thatAnnot.GetKind()) {
        found = true;
        continue;
      }
    }
    if (!found) {
      return false;
    }
  }
  for (const Annot &thatAnnot : that.annots_) {
    bool found = false;
    for (const Annot &thisAnnot : annots_) {
      if (thisAnnot == thatAnnot) {
        found = true;
        continue;
      }
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
    std::vector<DebugInfos> &&debug_infos)
  : Annot(Kind::CAML_FRAME)
  , allocs_(std::move(allocs))
  , debug_infos_(std::move(debug_infos))
{
}

// -----------------------------------------------------------------------------
bool CamlFrame::operator==(const CamlFrame &that) const
{
  return allocs_ == that.allocs_ && debug_infos_ == that.debug_infos_;
}

// -----------------------------------------------------------------------------
Probability::Probability(uint32_t n, uint32_t d)
  : Annot(Kind::PROBABILITY), n_(n), d_(d)
{
  assert(d_ != 0 && "invalid denumerator");
}

// -----------------------------------------------------------------------------
bool Probability::operator==(const Probability &that) const
{
  return n_ == that.n_ && d_ == that.d_;
}
