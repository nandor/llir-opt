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
    const char *argv0,
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
      Merge(*object);
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
          if (g->Is(Global::Kind::EXTERN)) {
            continue;
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

  // All done.
  return std::move(prog_);
}

// -----------------------------------------------------------------------------
void Linker::Merge(Prog &source)
{
  // Move the new externs.
  for (auto it = source.ext_begin(), end = source.ext_end(); it != end; ) {
    Extern *ext = &*it++;
    std::string extName(ext->getName());

    // Create a symbol mapped to the target name.
    if (Global *g = prog_->GetGlobal(extName)) {
      if (auto *gext = ::dyn_cast_or_null<Extern>(g); gext && ext->HasAlias()) {
        // Override a potential alias from a previous module.
        gext->replaceAllUsesWith(ext);
        gext->eraseFromParent();
        ext->removeFromParent();
        prog_->AddExtern(ext);
      } else {
        ext->replaceAllUsesWith(g);
        ext->eraseFromParent();
      }
    } else {
      // A new undefined symbol - record it.
      ext->removeFromParent();
      prog_->AddExtern(ext);
      unresolved_.insert(extName);
    }
  }

  for (auto it = source.begin(), end = source.end(); it != end; ) {
    Merge(*it++);
  }

  for (auto it = source.data_begin(), end = source.data_end(); it != end; ) {
    Merge(*it++);
  }
}

// -----------------------------------------------------------------------------
void Linker::Merge(Func &func)
{
  // Transfer the function.
  func.removeFromParent();
  prog_->AddFunc(&func);

  // Remove the missing symbols.
  Resolve(func);
  for (Block &block : func) {
    Resolve(block);
  }
}

// -----------------------------------------------------------------------------
void Linker::Merge(Data &data)
{
  // Concatenate or copy the data segment over.
  if (Data *prev = prog_->GetData(data.GetName())) {
    for (auto it = data.begin(); it != data.end(); ) {
      Object *object = &*it++;
      object->removeFromParent();
      prev->AddObject(object);
    }
    data.eraseFromParent();
  } else {
    data.removeFromParent();
    prog_->AddData(&data);
  }

  // Remove all missing symbols.
  for (Object &object : data) {
    for (Atom &atom : object) {
      Resolve(atom);
    }
  }
}

// -----------------------------------------------------------------------------
void Linker::Resolve(Global &global)
{
  unresolved_.erase(std::string(global.GetName()));
}
