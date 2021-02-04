// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/atom.h"
#include "core/block.h"
#include "core/cast.h"
#include "core/object.h"
#include "core/prog.h"

#include "linker.h"



// -----------------------------------------------------------------------------
Linker::Linker(
    llvm::StringRef argv0,
    std::vector<std::unique_ptr<Prog>> &&objects,
    std::vector<std::unique_ptr<Prog>> &&archives,
    std::string_view output)
  : objects_(std::move(objects))
  , archives_(std::move(archives))
  , prog_(std::make_unique<Prog>(output))
{
}

// -----------------------------------------------------------------------------
std::unique_ptr<Prog> Linker::Link()
{
  // Set of considered objects.
  std::set<llvm::StringRef> linked;

  // All objects will be part of the final executable.
  for (auto &object : objects_) {
    if (linked.insert(object->getName()).second) {
      if (!Merge(*object)) {
        return nullptr;
      }
    }
  }

  // Link archives to resolve missing symbols, as long as progress can be
  // made by resolving symbols and merging entire objects from the archive.
  {
    std::vector<Prog *> remaining;
    for (auto &prog : archives_) {
      remaining.push_back(&*prog);
    }

    bool progress;
    do {
      progress = false;
      for (auto it = remaining.begin(); it != remaining.end(); ) {
        Prog *archive = *it;

        // Skip the archive if it does not resolve any symbols.
        bool resolves = false;
        for (Global *g : archive->globals()) {
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
          if (linked.insert(archive->getName()).second) {
            Merge(*archive);
            progress = true;
          }
          it = remaining.erase(it);
        }
      }
    } while (progress);
  }

  // Resolve the aliases.
  for (auto it = prog_->ext_begin(); it != prog_->ext_end(); ) {
    Extern *ext = &*it++;
    if (Global *alias = ext->GetAlias()) {
      ext->replaceAllUsesWith(alias);
      if (ext->getName() == alias->getName()) {
        ext->eraseFromParent();
      }
    }
  }

  // Some sections need begin/end symbols.
  for (Data &data : prog_->data()) {
    // Find sections which have references to both start/end.
    std::string symbolStart = ("__start_" + data.getName()).str();
    auto *extStart = ::cast_or_null<Extern>(prog_->GetGlobal(symbolStart));
    std::string symbolEnd = ("__stop_" + data.getName()).str();
    auto *extEnd = ::cast_or_null<Extern>(prog_->GetGlobal(symbolEnd));
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

  // All done.
  return std::move(prog_);
}

// -----------------------------------------------------------------------------
bool Linker::Merge(Prog &source)
{
  // Move the new externs.
  for (auto it = source.ext_begin(), end = source.ext_end(); it != end; ) {
    Extern *currExt = &*it++;
    std::string extName(currExt->getName());

    // Create a symbol mapped to the target name.
    if (Global *g = prog_->GetGlobal(extName)) {
      if (auto *prevExt = ::cast_or_null<Extern>(g)) {
        if (prevExt->HasAlias()) {
          if (currExt->HasAlias()) {
            prevExt->replaceAllUsesWith(currExt);
            prevExt->eraseFromParent();
            currExt->removeFromParent();
            prog_->AddExtern(currExt);
          } else {
            currExt->replaceAllUsesWith(prevExt);
            currExt->removeFromParent();
          }
        } else {
          // The previous symbol is a weak alias or undefined - replace it.
          prevExt->replaceAllUsesWith(currExt);
          prevExt->eraseFromParent();
          currExt->removeFromParent();
          prog_->AddExtern(currExt);
        }
      } else {
        currExt->replaceAllUsesWith(g);
        currExt->eraseFromParent();
      }
    } else {
      // A new undefined symbol - record it.
      currExt->removeFromParent();
      prog_->AddExtern(currExt);
      if (!currExt->HasAlias()) {
        unresolved_.insert(extName);
      }
    }
  }

  for (auto it = source.begin(), end = source.end(); it != end; ) {
    if (!Merge(*it++)) {
      return false;
    }
  }

  for (auto it = source.data_begin(), end = source.data_end(); it != end; ) {
    if (!Merge(*it++)) {
      return false;
    }
  }

  for (auto it = source.xtor_begin(), end = source.xtor_end(); it != end; ) {
    if (!Merge(*it++)) {
      return false;
    }
  }

  return true;
}

// -----------------------------------------------------------------------------
bool Linker::Merge(Func &func)
{
  // Transfer the function.
  func.removeFromParent();
  prog_->AddFunc(&func);

  // Remove the missing symbols.
  Resolve(func);
  for (Block &block : func) {
    Resolve(block);
  }

  return true;
}

// -----------------------------------------------------------------------------
bool Linker::Merge(Data &data)
{
  // Concatenate or copy the data segment over.
  if (Data *prev = prog_->GetData(data.GetName())) {
    for (auto it = data.begin(); it != data.end(); ) {
      Object *object = &*it++;
      object->removeFromParent();
      prev->AddObject(object);
      for (Atom &atom : *object) {
        Resolve(atom);
      }
    }
    data.eraseFromParent();
  } else {
    data.removeFromParent();
    prog_->AddData(&data);

    // Remove all newly defined symbols.
    for (Object &object : data) {
      for (Atom &atom : object) {
        Resolve(atom);
      }
    }
  }

  return true;
}

// -----------------------------------------------------------------------------
bool Linker::Merge(Xtor &xtor)
{
  // Transfer the xtor.
  xtor.removeFromParent();
  prog_->AddXtor(&xtor);
  return true;
}

// -----------------------------------------------------------------------------
void Linker::Resolve(Global &global)
{
  unresolved_.erase(std::string(global.GetName()));
}
