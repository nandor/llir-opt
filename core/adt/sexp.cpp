// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <cassert>

#include <llvm/Support/ErrorHandling.h>

#include "core/adt/sexp.h"



// -----------------------------------------------------------------------------
void SExp::Number::print(llvm::raw_ostream &os) const
{
  os << v_;
}

// -----------------------------------------------------------------------------
void SExp::String::print(llvm::raw_ostream &os) const
{
  os << v_;
}

// -----------------------------------------------------------------------------
SExp::Number *SExp::List::AddNumber(int64_t v)
{
  v_.emplace_back(v);
  return v_.rbegin()->AsNumber();
}

// -----------------------------------------------------------------------------
SExp::String *SExp::List::AddString(const std::string &v)
{
  v_.emplace_back(v);
  return v_.rbegin()->AsString();
}

// -----------------------------------------------------------------------------
SExp::List *SExp::List::AddList()
{
  v_.emplace_back();
  return v_.rbegin()->AsList();
}

// -----------------------------------------------------------------------------
void SExp::List::print(llvm::raw_ostream &os) const
{
  os << "(";
  for (size_t i = 0; i < v_.size(); ++i) {
    if (i != 0) {
      os << " ";
    }
    v_[i].print(os);
  }
  os << ")";
}

// -----------------------------------------------------------------------------
SExp::~SExp()
{
}

// -----------------------------------------------------------------------------
const SExp::Number *SExp::AsNumber() const
{
  return s_->K == Kind::NUMBER ? &s_->N : nullptr;
}

// -----------------------------------------------------------------------------
SExp::Number *SExp::AsNumber()
{
  return s_->K == Kind::NUMBER ? &s_->N : nullptr;
}

// -----------------------------------------------------------------------------
const SExp::String *SExp::AsString() const
{
  return s_->K == Kind::STRING ? &s_->S : nullptr;
}

// -----------------------------------------------------------------------------
SExp::String *SExp::AsString()
{
  return s_->K == Kind::STRING ? &s_->S : nullptr;
}

// -----------------------------------------------------------------------------
const SExp::List *SExp::AsList() const
{
  return s_->K == Kind::LIST ? &s_->L : nullptr;
}

// -----------------------------------------------------------------------------
SExp::List *SExp::AsList()
{
  return s_->K == Kind::LIST ? &s_->L : nullptr;
}

// -----------------------------------------------------------------------------
void SExp::print(llvm::raw_ostream &os) const
{
  switch (s_->K) {
    case Kind::NUMBER: s_->N.print(os); return;
    case Kind::STRING: s_->S.print(os); return;
    case Kind::LIST:   s_->L.print(os); return;
  }
  llvm_unreachable("invalid sexp kind");
}

// -----------------------------------------------------------------------------
SExp::Storage::Storage(const Storage &that)
{
  switch (that.K) {
    case Kind::NUMBER: new (&N) Number(that.N); return;
    case Kind::STRING: new (&S) String(that.S); return;
    case Kind::LIST:   new (&L) List(that.L); return;
  }
  llvm_unreachable("invalid sexp kind");
}


// -----------------------------------------------------------------------------
SExp::Storage::~Storage()
{
  switch (K) {
    case Kind::NUMBER: N.~Number(); return;
    case Kind::STRING: S.~String(); return;
    case Kind::LIST:   L.~List(); return;
  }
  llvm_unreachable("invalid sexp kind");
}
