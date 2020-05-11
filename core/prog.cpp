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
void Prog::AddFunc(Func *func, Func *before)
{
  if (before == nullptr) {
    funcs_.push_back(func);
  } else {
    funcs_.insert(before->getIterator(), func);
  }
}

// -----------------------------------------------------------------------------
void Prog::AddExtern(Extern *ext, Extern *before)
{
  if (before == nullptr) {
    externs_.push_back(ext);
  } else {
    externs_.insert(before->getIterator(), ext);
  }
}

// -----------------------------------------------------------------------------
void Prog::AddData(Data *data, Data *before)
{
  if (before == nullptr) {
    datas_.push_back(data);
  } else {
    datas_.insert(before->getIterator(), data);
  }
}

// -----------------------------------------------------------------------------
void Prog::remove(iterator it)
{
  funcs_.remove(it);
}

// -----------------------------------------------------------------------------
void Prog::erase(iterator it)
{
  funcs_.erase(it);
}

// -----------------------------------------------------------------------------
void Prog::remove(ext_iterator it)
{
  externs_.remove(it);
}

// -----------------------------------------------------------------------------
void Prog::erase(ext_iterator it)
{
  externs_.erase(it);
}

// -----------------------------------------------------------------------------
void Prog::remove(data_iterator it)
{
  datas_.remove(it);
}

// -----------------------------------------------------------------------------
void Prog::erase(data_iterator it)
{
  datas_.erase(it);
}

// -----------------------------------------------------------------------------
Global *Prog::GetGlobalOrExtern(const std::string_view name)
{
  auto it = globals_.find(name);
  if (it != globals_.end()) {
    return it->second;
  }
  Extern *e = new Extern(name);
  externs_.push_back(e);
  return e;
}

// -----------------------------------------------------------------------------
Extern *Prog::GetExtern(const std::string_view name)
{
  auto it = globals_.find(name);
  if (it == globals_.end()) {
    return nullptr;
  }
  return ::dyn_cast_or_null<Extern>(it->second);
}

// -----------------------------------------------------------------------------
Data *Prog::GetOrCreateData(const std::string_view name)
{
  if (auto *data = GetData(name)) {
    return data;
  }
  Data *data = new Data(name);
  datas_.push_back(data);
  return data;
}

// -----------------------------------------------------------------------------
Data *Prog::GetData(const std::string_view name)
{
  for (auto &data : datas_) {
    if (data.GetName() == name) {
      return &data;
    }
  }
  return nullptr;
}

// -----------------------------------------------------------------------------
Global *Prog::GetGlobal(const std::string_view name)
{
  auto it = globals_.find(name);
  if (it == globals_.end()) {
    return nullptr;
  }
  return it->second;
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
void Prog::insertGlobal(Global *g)
{
  auto it = globals_.emplace(g->GetName(), g);
  if (it.second) {
    return;
  }

  Global *prev = it.first->second;
  if (auto *ext = ::dyn_cast_or_null<Extern>(prev)) {
    // Delete the extern which was replaced.
    ext->replaceAllUsesWith(g);
    ext->eraseFromParent();

    // Try to insert the symbol again.
    auto st = globals_.emplace(g->GetName(), g);
    assert(st.second && "symbol not inserted");
    return;
  }

  if (g->IsHidden()) {
    std::string orig = g->name_;
    static unsigned unique;
    do {
      g->name_ = orig + "$static" + std::to_string(unique++);
    } while (!globals_.emplace(g->GetName(), g).second);
  } else {
    if (prev->IsWeak()) {
      prev->replaceAllUsesWith(g);
      prev->eraseFromParent();
      auto st = globals_.emplace(g->GetName(), g);
      assert(st.second && "symbol not inserted");
    } else {
      llvm::report_fatal_error("duplicate symbol");
    }
  }
}

// -----------------------------------------------------------------------------
void Prog::removeGlobalName(std::string_view name)
{
  auto it = globals_.find(name);
  assert(it != globals_.end() && "symbol not found");
  globals_.erase(it);
}

