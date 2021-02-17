// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/Support/Error.h>

#include "core/atom.h"
#include "core/block.h"
#include "core/cast.h"
#include "core/error.h"
#include "core/object.h"
#include "core/prog.h"

#include "linker.h"



// -----------------------------------------------------------------------------
Linker::Unit::Unit(std::unique_ptr<Prog> &&prog)
  : kind_(Kind::LLIR)
{
  new (&s_.P) std::unique_ptr<Prog>(std::move(prog));
}

// -----------------------------------------------------------------------------
Linker::Unit::Unit(const Bitcode &data)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
Linker::Unit::Unit(const Object &object)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
Linker::Unit::Unit(const Data &data)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
Linker::Unit::Unit(Unit &&that)
  : kind_(that.kind_)
{
  switch (that.kind_) {
    case Unit::Kind::LLIR: {
      new (&s_.P) std::unique_ptr<Prog>(std::move(that.s_.P));
      return;
    }
    case Unit::Kind::BITCODE: {
      llvm_unreachable("not implemented");
    }
    case Unit::Kind::OBJECT: {
      llvm_unreachable("not implemented");
    }
    case Unit::Kind::DATA: {
      llvm_unreachable("not implemented");
    }
  }
  llvm_unreachable("invalid unit kind");
}

// -----------------------------------------------------------------------------
Linker::Unit::~Unit()
{
  switch (kind_) {
    case Unit::Kind::LLIR: {
      s_.P.~unique_ptr();
      return;
    }
    case Unit::Kind::BITCODE: {
      llvm_unreachable("not implemented");
    }
    case Unit::Kind::OBJECT: {
      llvm_unreachable("not implemented");
    }
    case Unit::Kind::DATA: {
      llvm_unreachable("not implemented");
    }
  }
  llvm_unreachable("invalid unit kind");
}

// -----------------------------------------------------------------------------
Linker::Unit &Linker::Unit::operator=(Unit &&)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
llvm::Error Linker::LinkObject(Unit &&unit)
{
  switch (unit.kind_) {
    case Unit::Kind::LLIR: {
      auto &prog = *unit.s_.P;
      if (linked_.insert(prog.getName()).second) {
        Resolve(prog);
        units_.emplace_back(std::move(unit));
      }
      return llvm::Error::success();
    }
    case Unit::Kind::BITCODE: {
      llvm_unreachable("not implemented");
    }
    case Unit::Kind::OBJECT: {
      llvm_unreachable("not implemented");
    }
    case Unit::Kind::DATA: {
      llvm_unreachable("not implemented");
    }
  }
  llvm_unreachable("invalid unit kind");
}

// -----------------------------------------------------------------------------
llvm::Error Linker::LinkGroup(std::vector<Linker::Unit> &&units)
{
  // Link archives to resolve missing symbols, as long as progress can be
  // made by resolving symbols and merging entire objects from the archive.
  {
    bool progress;
    do {
      progress = false;
      for (auto it = units.begin(); it != units.end(); ) {
        switch (it->kind_) {
          case Unit::Kind::LLIR: {
            Prog &p = *it->s_.P;

            // Skip the archive if it does not resolve any symbols.
            bool resolves = false;
            for (Global *g : p.globals()) {
              if (auto *ext = ::cast_or_null<Extern>(g)) {
                if (!ext->HasAlias()) {
                  continue;
                }
              }
              if (unresolved_.count(std::string(g->getName()))) {
                resolves = true;
                break;
              }
            }
            if (!resolves) {
              ++it;
            } else {
              // Merge the new object from the archive.
              if (linked_.insert(p.getName()).second) {
                Resolve(p);
                units_.emplace_back(std::move(*it));
                progress = true;
              }
              it = units.erase(it);
            }
            continue;
          }
          case Unit::Kind::BITCODE: {
            llvm_unreachable("not implemented");
          }
          case Unit::Kind::OBJECT: {
            llvm_unreachable("not implemented");
          }
          case Unit::Kind::DATA: {
            llvm_unreachable("not implemented");
          }
        }
        llvm_unreachable("invalid unit kind");
      }
    } while (progress);
  }
  return llvm::Error::success();
}

// -----------------------------------------------------------------------------
llvm::Expected<Linker::LinkResult> Linker::Link()
{
  auto prog = std::make_unique<Prog>(output_);
  for (auto &&module : LTO()) {
    Merge(*prog, *module);
  }

  // Resolve the aliases.
  for (auto it = prog->ext_begin(); it != prog->ext_end(); ) {
    Extern *ext = &*it++;
    if (Global *alias = ext->GetAlias()) {
      ext->replaceAllUsesWith(alias);
      if (ext->getName() == alias->getName()) {
        ext->eraseFromParent();
      }
    }
  }

  // Some sections need begin/end symbols.
  for (Data &data : prog->data()) {
    // Find sections which have references to both start/end.
    std::string symbolStart = ("__start_" + data.getName()).str();
    auto *extStart = ::cast_or_null<Extern>(prog->GetGlobal(symbolStart));
    std::string symbolEnd = ("__stop_" + data.getName()).str();
    auto *extEnd = ::cast_or_null<Extern>(prog->GetGlobal(symbolEnd));
    if (!extStart || !extEnd) {
      continue;
    }

    // Concatenate all items into a single object.
    Object *object = new Object();
    for (auto ot = data.begin(); ot != data.end(); ++ot) {
      Object *source = &*ot++;
      for (auto at = source->begin(); at != source->end(); ++at) {
        Atom *atom = &*at++;
        atom->removeFromParent();
        object->AddAtom(atom);
      }
      source->eraseFromParent();
    }

    data.AddObject(object);
    if (object->empty()) {
      llvm_unreachable("not implemented");
    } else {
      extStart->replaceAllUsesWith(&*object->begin());
      extStart->eraseFromParent();
    }
    Atom *endAtom = new Atom(("__end_" + data.getName()).str());
    object->AddAtom(endAtom);
    extEnd->replaceAllUsesWith(endAtom);
    extEnd->eraseFromParent();
  }

  return LinkResult{ std::move(prog), std::move(files_) };
}

// -----------------------------------------------------------------------------
std::vector<std::unique_ptr<Prog>> Linker::LTO()
{
  std::vector<std::unique_ptr<Prog>> objects;
  for (auto &&unit : units_) {
    switch (unit.kind_) {
      case Unit::Kind::LLIR: {
        objects.emplace_back(std::move(unit.s_.P));
        continue;
      }
      case Unit::Kind::BITCODE: {
        llvm_unreachable("not implemented");
      }
      case Unit::Kind::OBJECT: {
        llvm_unreachable("not implemented");
      }
      case Unit::Kind::DATA: {
        llvm_unreachable("not implemented");
      }
    }
    llvm_unreachable("invalid unit kind");
  }
  return objects;
}

// -----------------------------------------------------------------------------
void Linker::Resolve(Prog &p)
{
  for (Extern &ext : p.externs()) {
    std::string name(ext.getName());
    if (!resolved_.count(name) && !ext.HasAlias()) {
      unresolved_.insert(name);
    }
  }

  for (Func &func : p) {
    Resolve(std::string(func.GetName()));
    for (Block &block : func) {
      Resolve(std::string(block.GetName()));
    }
  }

  for (Data &data : p.data()) {
    for (Object &object : data) {
      for (Atom &atom : object) {
        Resolve(std::string(atom.GetName()));
      }
    }
  }
}

// -----------------------------------------------------------------------------
void Linker::Resolve(const std::string &name)
{
  unresolved_.erase(name);
  resolved_.insert(name);
}

// -----------------------------------------------------------------------------
bool Linker::Merge(Prog &dest, Prog &source)
{
  // Move the new externs.
  for (auto it = source.ext_begin(), end = source.ext_end(); it != end; ) {
    Extern *currExt = &*it++;
    std::string extName(currExt->getName());

    // Create a symbol mapped to the target name.
    if (Global *g = dest.GetGlobal(extName)) {
      if (auto *prevExt = ::cast_or_null<Extern>(g)) {
        if (prevExt->HasAlias()) {
          if (currExt->HasAlias()) {
            prevExt->replaceAllUsesWith(currExt);
            prevExt->eraseFromParent();
            currExt->removeFromParent();
            dest.AddExtern(currExt);
          } else {
            currExt->replaceAllUsesWith(prevExt);
            currExt->removeFromParent();
          }
        } else {
          // The previous symbol is a weak alias or undefined - replace it.
          prevExt->replaceAllUsesWith(currExt);
          prevExt->eraseFromParent();
          currExt->removeFromParent();
          dest.AddExtern(currExt);
        }
      } else {
        currExt->replaceAllUsesWith(g);
        currExt->eraseFromParent();
      }
    } else {
      // A new undefined symbol - record it.
      currExt->removeFromParent();
      dest.AddExtern(currExt);
    }
  }

  for (auto it = source.begin(), end = source.end(); it != end; ) {
    if (!Merge(dest, *it++)) {
      return false;
    }
  }

  for (auto it = source.data_begin(), end = source.data_end(); it != end; ) {
    if (!Merge(dest, *it++)) {
      return false;
    }
  }

  for (auto it = source.xtor_begin(), end = source.xtor_end(); it != end; ) {
    if (!Merge(dest, *it++)) {
      return false;
    }
  }

  return true;
}

// -----------------------------------------------------------------------------
bool Linker::Merge(Prog &dest, Func &func)
{
  if (func.IsWeak()) {
    if (auto *g = dest.GetGlobal(func.getName())) {
      if (!g->Is(Global::Kind::EXTERN)) {
        return true;
      }
    }
  }
  // Transfer the function.
  func.removeFromParent();
  dest.AddFunc(&func);
  return true;
}

// -----------------------------------------------------------------------------
bool Linker::Merge(Prog &dest, Data &data)
{
  // Concatenate or copy the data segment over.
  if (Data *prev = dest.GetData(data.GetName())) {
    for (auto it = data.begin(); it != data.end(); ) {
      Object *object = &*it++;
      object->removeFromParent();
      prev->AddObject(object);
    }
    data.eraseFromParent();
  } else {
    data.removeFromParent();
    dest.AddData(&data);
  }

  return true;
}

// -----------------------------------------------------------------------------
bool Linker::Merge(Prog &dest, Xtor &xtor)
{
  // Transfer the xtor.
  xtor.removeFromParent();
  dest.AddXtor(&xtor);
  return true;
}
