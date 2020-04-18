// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/cast.h"
#include "core/data.h"
#include "core/func.h"
#include "core/block.h"
#include "core/prog.h"



// -----------------------------------------------------------------------------
Prog::Prog()
{
}

// -----------------------------------------------------------------------------
Prog::~Prog()
{
}

// -----------------------------------------------------------------------------
void Prog::erase(iterator it)
{
  auto symIt = symbols_.find(it->GetName());
  funcs_.erase(it);
  if (symIt != symbols_.end()) {
    symbols_.erase(symIt);
  }
}

// -----------------------------------------------------------------------------
void Prog::erase(ext_iterator it)
{
  auto symIt = symbols_.find(it->GetName());
  externs_.erase(it);
  if (symIt != symbols_.end()) {
    symbols_.erase(symIt);
  }
}

// -----------------------------------------------------------------------------
void Prog::AddFunc(Func *func, Func *before)
{
  if (before == nullptr) {
    funcs_.push_back(func);
  } else {
    funcs_.insert(before->getIterator(), func);
  }
}

// -----------------------------------------------------------------------------
Atom *Prog::CreateAtom(const std::string_view name)
{
  auto it = symbols_.find(name);
  Global *prev = nullptr;
  if (it != symbols_.end()) {
    prev = it->second;
    if (::dyn_cast_or_null<Extern>(prev)) {
      symbols_.erase(it);
    } else {
      llvm::report_fatal_error("Duplicate atom " + std::string(name));
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
    if (::dyn_cast_or_null<Extern>(prev)) {
      symbols_.erase(it);
    } else {
      llvm::report_fatal_error("Duplicate function " + std::string(name));
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
Global *Prog::GetGlobal(const std::string_view name)
{
  auto it = symbols_.find(name);
  if (it != symbols_.end()) {
    return it->second;
  }

  Extern *e = new Extern(this, name);
  externs_.push_back(e);
  symbols_.emplace(e->GetName(), e);
  return e;
}

// -----------------------------------------------------------------------------
Extern *Prog::GetExtern(const std::string_view name)
{
  auto it = symbols_.find(name);
  if (it == symbols_.end()) {
    return nullptr;
  }
  return ::dyn_cast_or_null<Extern>(it->second);
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
Data *Prog::CreateData(const std::string_view name)
{
  for (auto &data : datas_) {
    if (data.GetName() == name) {
      return &data;
    }
  }

  Data *data = new Data(this, name);
  datas_.push_back(data);
  return data;
}

// -----------------------------------------------------------------------------
llvm::iterator_range<Prog::const_iterator> Prog::funcs() const
{
  return llvm::make_range(begin(), end());
}

// -----------------------------------------------------------------------------
llvm::iterator_range<Prog::iterator> Prog::funcs()
{
  return llvm::make_range(begin(), end());
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

// -----------------------------------------------------------------------------
llvm::iterator_range<Prog::const_data_iterator> Prog::data() const
{
  return llvm::make_range(data_begin(), data_end());
}

// -----------------------------------------------------------------------------
llvm::iterator_range<Prog::data_iterator> Prog::data()
{
  return llvm::make_range(data_begin(), data_end());
}

// -----------------------------------------------------------------------------
void llvm::ilist_traits<Func>::addNodeToList(Func *func)
{
}

// -----------------------------------------------------------------------------
void llvm::ilist_traits<Func>::removeNodeFromList(Func *func)
{
}

// -----------------------------------------------------------------------------
void llvm::ilist_traits<Func>::transferNodesFromList(
    ilist_traits &from,
    instr_iterator first,
    instr_iterator last)
{
}

// -----------------------------------------------------------------------------
void llvm::ilist_traits<Func>::deleteNode(Func *func)
{
  delete func;
}
