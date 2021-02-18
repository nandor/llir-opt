// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/Support/Error.h>
#include <llvm/CodeGen/CommandFlags.h>

#include "core/atom.h"
#include "core/block.h"
#include "core/cast.h"
#include "core/error.h"
#include "core/object.h"
#include "core/parser.h"
#include "core/prog.h"

#include "linker.h"



// -----------------------------------------------------------------------------
Linker::Unit::Unit(std::unique_ptr<Prog> &&prog)
  : kind_(Kind::LLIR)
{
  new (&s_.P) std::unique_ptr<Prog>(std::move(prog));
}

// -----------------------------------------------------------------------------
Linker::Unit::Unit(std::unique_ptr<llvm::lto::InputFile> &&bitcode)
  : kind_(Kind::BITCODE)
{
  new (&s_.B) std::unique_ptr<llvm::lto::InputFile>(std::move(bitcode));
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
      new (&s_.B) std::unique_ptr<llvm::lto::InputFile>(std::move(that.s_.B));
      return;
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
      s_.B.~unique_ptr();
      return;
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
      auto &obj = *unit.s_.B;
      for (auto s : obj.getComdatTable()) {
        llvm_unreachable("not implemented");
      }

      for (const auto &sym : obj.symbols()) {
        if (sym.isUndefined()) {
          std::string name(sym.getName());
          if (!resolved_.count(name)) {
            unresolved_.insert(name);
          }
        } else {
          Resolve(sym.getName().str());
        }
      }

      for (auto l : obj.getDependentLibraries()) {
        llvm_unreachable("not implemented");
      }

      units_.emplace_back(std::move(unit));
      return llvm::Error::success();
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
            auto &p = *it->s_.P;
            if (!Resolves(p)) {
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
            auto &b = *it->s_.B;
            if (!Resolves(b)) {
              ++it;
            } else {
              // Merge the new object from the archive.
              if (linked_.insert(b.getName()).second) {
                Resolve(b);
                units_.emplace_back(std::move(*it));
                progress = true;
              }
              it = units.erase(it);
            }
            continue;
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
bool Linker::Resolves(Prog &p)
{
  for (Global *g : p.globals()) {
    if (auto *ext = ::cast_or_null<Extern>(g)) {
      if (!ext->HasAlias()) {
        continue;
      }
    }
    if (unresolved_.count(std::string(g->getName()))) {
      return true;
    }
  }
  return false;
}

// -----------------------------------------------------------------------------
bool Linker::Resolves(llvm::lto::InputFile &obj)
{
  for (auto s : obj.getComdatTable()) {
    llvm_unreachable("not implemented");
  }

  for (const auto &sym : obj.symbols()) {
    if (!sym.isUndefined()) {
      if (unresolved_.count(sym.getName().str())) {
        return true;
      }
    }
  }

  for (auto l : obj.getDependentLibraries()) {
    llvm_unreachable("not implemented");
  }
  return false;
}

// -----------------------------------------------------------------------------
llvm::Expected<Linker::LinkResult> Linker::Link()
{
  auto prog = std::make_unique<Prog>(output_);
  auto modulesOrError = Collect();
  if (!modulesOrError) {
    return std::move(modulesOrError.takeError());
  }
  for (auto &&module : modulesOrError.get()) {
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
llvm::Expected<std::vector<std::unique_ptr<Prog>>> Linker::Collect()
{
  std::vector<std::unique_ptr<Prog>> objects;
  std::vector<std::unique_ptr<llvm::lto::InputFile>> bitcodes;
  for (auto &&unit : units_) {
    switch (unit.kind_) {
      case Unit::Kind::LLIR: {
        objects.emplace_back(std::move(unit.s_.P));
        continue;
      }
      case Unit::Kind::BITCODE: {
        bitcodes.emplace_back(std::move(unit.s_.B));
        continue;
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

  if (!bitcodes.empty()) {
    auto moduleOrError = LTO(std::move(bitcodes));
    if (!moduleOrError) {
      return std::move(moduleOrError.takeError());
    }
    for (auto &&object : moduleOrError.get()) {
      objects.emplace_back(std::move(object));
    }
  }

  return std::move(objects);
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
void Linker::Resolve(llvm::lto::InputFile &obj)
{
  for (auto s : obj.getComdatTable()) {
    llvm_unreachable("not implemented");
  }

  for (const auto &sym : obj.symbols()) {
    if (!sym.isUndefined()) {
      Resolve(sym.getName().str());
    }
  }

  for (auto l : obj.getDependentLibraries()) {
    llvm_unreachable("not implemented");
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

// -----------------------------------------------------------------------------
llvm::MCTargetOptions Linker::CreateMCTargetOptions()
{
  llvm::MCTargetOptions Options;
  return Options;
}

// -----------------------------------------------------------------------------
llvm::TargetOptions Linker::CreateTargetOptions()
{
  llvm::TargetOptions Options;
  Options.MCOptions = CreateMCTargetOptions();
  return Options;
}

// -----------------------------------------------------------------------------
llvm::lto::Config Linker::CreateLTOConfig()
{
  llvm::lto::Config c;
  c.Options = CreateTargetOptions();
  c.CGFileType = llvm::CGFT_AssemblyFile;
  return c;
}

// -----------------------------------------------------------------------------
llvm::Expected<std::vector<std::unique_ptr<Prog>>> Linker::LTO(
    std::vector<std::unique_ptr<llvm::lto::InputFile>> &&modules)
{
  auto backend = llvm::lto::createInProcessThinBackend(
        llvm::heavyweight_hardware_concurrency(1)
  );
  auto opt = std::make_unique<llvm::lto::LTO>(CreateLTOConfig(), backend);

  for (auto &&obj : modules) {
    auto objSyms = obj->symbols();
    std::vector<llvm::lto::SymbolResolution> resols(objSyms.size());

    // Provide a resolution to the LTO API for each symbol.
    for (size_t i = 0, n = objSyms.size(); i != n; ++i) {
      const auto &objSym = objSyms[i];
      auto &r = resols[i];
      r.Prevailing = !objSym.isUndefined();
      r.VisibleToRegularObj = true;
      r.FinalDefinitionInLinkageUnit = true;
      r.LinkerRedefined = false;
    }

    if (auto err = opt->add(std::move(obj), resols)) {
      return std::move(err);
    }
  }

  const size_t tasks = opt->getMaxTasks();
  std::vector<llvm::SmallString<0>> outputs(tasks);

  auto stream = [&](size_t task)
  {
    return std::make_unique<llvm::lto::NativeObjectStream>(
        std::make_unique<llvm::raw_svector_ostream>(outputs[task])
    );
  };

  llvm::lto::NativeObjectCache cache;
  if (auto err = opt->run(stream, cache)) {
    return std::move(err);
  }

  std::vector<std::unique_ptr<Prog>> progs;
  for (unsigned i = 0, n = outputs.size(); i < n; ++i) {
    auto name = "lto." + llvm::Twine(i);
    auto prog = Parser(outputs[i], name.str()).Parse();
    if (!prog) {
      return MakeError("cannot parse LTO output");
    }
    progs.emplace_back(std::move(prog));
  }
  return std::move(progs);
}
