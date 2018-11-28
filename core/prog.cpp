// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/data.h"
#include "core/func.h"
#include "core/block.h"
#include "core/prog.h"



// -----------------------------------------------------------------------------
Prog::Prog()
  : data_(new Data(this, "data"))
  , bss_(new Data(this, "bss"))
  , const_(new Data(this, "const"))
{
}

// -----------------------------------------------------------------------------
Atom *Prog::CreateAtom(const std::string_view name)
{
  auto it = symbols_.find(name);
  Global *prev = nullptr;
  if (it != symbols_.end()) {
    prev = it->second;
    if (prev->IsDefined()) {
      throw std::runtime_error("Duplicate atom " + std::string(name));
    } else {
      symbols_.erase(it);
    }
  }
  
  Atom *atom = new Atom(name);
  symbols_.emplace(atom->GetName(), atom);

  if (prev) {
    prev->replaceAllUsesWith(atom);
  }
  return atom;
}

// -----------------------------------------------------------------------------
Func *Prog::CreateFunc(const std::string_view name)
{
  auto it = symbols_.find(name);
  Global *prev = nullptr;
  if (it != symbols_.end()) {
    prev = it->second;
    if (prev->IsDefined()) {
      throw std::runtime_error("Duplicate function " + std::string(name));
    } else {
      symbols_.erase(it);
    }
  }

  Func *f = new Func(this, name);
  funcs_.push_back(f);
  symbols_.emplace(f->GetName(), f);

  if (prev) {
    prev->replaceAllUsesWith(f);
  }
  return f;
}

// -----------------------------------------------------------------------------
Extern *Prog::CreateExtern(const std::string_view name)
{
  auto it = symbols_.find(name);
  Global *prev = nullptr;
  if (it != symbols_.end()) {
    prev = it->second;
    if (prev->IsDefined()) {
      throw std::runtime_error("Duplicate extern " + std::string(name));
    } else {
      symbols_.erase(it);
    }
  }
  
  Extern *e = new Extern(name);
  externs_.push_back(e);
  symbols_.emplace(e->GetName(), e);

  if (prev) {
    prev->replaceAllUsesWith(e);
  }
  return e;
}

// -----------------------------------------------------------------------------
Global *Prog::GetGlobal(const std::string_view name)
{
  auto it = symbols_.find(name);
  if (it != symbols_.end()) {
    return it->second;
  }

  auto sym = new Symbol(name);
  symbols_.emplace(sym->GetName(), sym);
  return sym;
}

// -----------------------------------------------------------------------------
Expr *Prog::CreateSymbolOffset(Global *sym, int64_t offset)
{
  return new SymbolOffsetExpr(sym, offset);
}

// -----------------------------------------------------------------------------
ConstantInt *Prog::CreateInt(int64_t v)
{
  return new ConstantInt(v);
}

// -----------------------------------------------------------------------------
ConstantFloat *Prog::CreateFloat(double v)
{
  return new ConstantFloat(v);
}

// -----------------------------------------------------------------------------
ConstantReg *Prog::CreateReg(ConstantReg::Kind v)
{
  return new ConstantReg(v);
}

// -----------------------------------------------------------------------------
llvm::iterator_range<Prog::const_ext_iterator> Prog::externs() const
{
  return llvm::make_range(ext_begin(), ext_end());
}

// -----------------------------------------------------------------------------
llvm::iterator_range<Prog::ext_iterator> Prog::externs()
{
  return llvm::make_range(ext_begin(), ext_end());
}
