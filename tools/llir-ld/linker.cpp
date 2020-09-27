// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/prog.h"
#include "core/block.h"
#include "core/object.h"
#include "core/atom.h"

#include "linker.h"



// -----------------------------------------------------------------------------
static void Merge(Prog &dst, Prog &src, std::set<std::string> &missing)
{
  // Move the new externs.
  for (auto it = src.ext_begin(), end = src.ext_end(); it != end; ) {
    Extern *ext = &*it++;
    if (auto *alias = ext->GetAlias()) {
      switch (alias->GetKind()) {
        case Global::Kind::FUNC: {
          llvm_unreachable("not implemented");
        }
        case Global::Kind::ATOM: {
          llvm_unreachable("not implemented");
        }
        case Global::Kind::BLOCK: {
          llvm_unreachable("not implemented");
        }
        case Global::Kind::EXTERN: {
          llvm_unreachable("not implemented");
        }
      }
    } else {
      if (auto *g = dst.GetGlobal(ext->getName())) {
        // Extern replaced with previous definition.
        ext->replaceAllUsesWith(g);
        ext->eraseFromParent();
      } else {
        // A new undefined symbol - record it.
        ext->removeFromParent();
        dst.AddExtern(ext);
        missing.insert(std::string(ext->getName()));
      }
    }
  }

  for (auto it = src.begin(), end = src.end(); it != end; ) {
    // Move the function.
    Func *func = &*it++;
    func->removeFromParent();
    dst.AddFunc(func);
    // Remove the missing symbols.
    missing.erase(std::string(func->GetName()));
    for (const Block &block : *func) {
      missing.erase(std::string(block.GetName()));
    }
  }

  for (auto it = src.data_begin(), end = src.data_end(); it != end; ) {
    // Move the data segment.
    Data *data = &*it++;
    if (Data *prev = dst.GetData(data->GetName())) {
      for (auto it = data->begin(); it != data->end(); ) {
        Object *object = &*it++;
        object->removeFromParent();
        prev->AddObject(object);
      }
      data->eraseFromParent();
    } else {
      data->removeFromParent();
      dst.AddData(data);
    }
    // Remove all missing symbols.
    for (const Object &object : *data) {
      for (const Atom &atom : object) {
        missing.erase(std::string(atom.GetName()));
      }
    }
  }
}

// -----------------------------------------------------------------------------
std::unique_ptr<Prog> Linker::Link(
    const std::vector<std::unique_ptr<Prog>> &objects,
    const std::vector<std::unique_ptr<Prog>> &archives,
    std::string_view output,
    const std::set<std::string> &entries)
{
  // Set of considered objects.
  std::set<llvm::StringRef> linked;

  // Set of missing symbols.
  std::set<std::string> missing;
  for (auto &entry : entries) {
    missing.insert(entry);
  }

  // All objects will be part of the final executable.
  auto prog = std::make_unique<Prog>(output);
  for (auto &object : objects) {
    if (linked.insert(object->getName()).second) {
      Merge(*prog, *object, missing);
    }
  }

  // Identify the initial set of missing symbols. Afterwards, try to
  // resolve them by transferring modules from the subsequent archives.
  {
    std::vector<Prog *> remaining;
    for (auto &prog : archives) {
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
          if (missing.count(std::string(g->getName()))) {
            resolves = true;
            break;
          }
        }
        if (!resolves) {
          ++it;
        } else {
          // Merge the new object from the archive.
          if (linked.insert(archive->getName()).second) {
            Merge(*prog, *archive, missing);
            progress = true;
          }
          it = remaining.erase(it);
        }
      }
    } while (progress);
  }

  return std::move(prog);
}
