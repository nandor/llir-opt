// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/data.h"
#include "core/prog.h"



// -----------------------------------------------------------------------------
Atom *Data::CreateAtom(const std::string_view name)
{
  Atom *atom = prog_->CreateAtom(name);
  atoms_.push_back(atom);
  return atom;
}
