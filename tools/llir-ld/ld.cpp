// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <sstream>
#include <set>
#include <unordered_set>
#include <queue>

#include <llvm/ADT/PointerUnion.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/InitLLVM.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/Program.h>
#include <llvm/Support/ToolOutputFile.h>
#include <llvm/Support/WithColor.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/prog.h"
#include "core/util.h"
#include "core/func.h"
#include "core/data.h"
#include "core/printer.h"
#include "core/bitcode.h"

namespace cl = llvm::cl;
namespace sys = llvm::sys;
using WithColor = llvm::WithColor;
using StringRef = llvm::StringRef;



/**
 * Enumeration of optimisation levels.
 */
enum class OptLevel {
  /// No optimisations.
  O0,
  /// Simple optimisations.
  O1,
  /// Aggressive optimisations.
  O2,
  /// All optimisations.
  O3
};

/**
 * Enumeration of output formats.
 */
enum class OutputType {
  EXE,
  OBJ,
  ASM,
  LLIR,
  LLBC
};

// -----------------------------------------------------------------------------
static cl::list<std::string>
optInput(cl::Positional, cl::desc("<input>"), cl::OneOrMore);

static cl::opt<std::string>
optOutput("o", cl::desc("output"), cl::init("-"));

static cl::list<std::string>
optLibPaths("L", cl::desc("library path"), cl::Prefix);

static cl::list<std::string>
optLibraries("l", cl::desc("libraries"), cl::Prefix);

static cl::opt<std::string>
optEntry("e", cl::desc("entry point"), cl::init("main"));

static cl::opt<bool>
optExportDynamic("E", cl::init(false), cl::ZeroOrMore);

static cl::opt<bool>
optRelocatable("r", cl::desc("relocatable"));

static cl::opt<bool>
optShared("shared", cl::desc("build a shared library"));

static cl::opt<bool>
optStatic("static", cl::desc("build a static executable"));

static cl::opt<std::string>
optDynamicLinker("dynamic-linker", cl::desc("path to the dynamic linker"));

static cl::opt<OptLevel>
optOptLevel(
  cl::desc("optimisation level:"),
  cl::values(
    clEnumValN(OptLevel::O0, "O0", "No optimizations"),
    clEnumValN(OptLevel::O1, "O1", "Simple optimisations"),
    clEnumValN(OptLevel::O2, "O2", "Aggressive optimisations"),
    clEnumValN(OptLevel::O3, "O3", "All optimisations")
  ),
  cl::init(OptLevel::O0)
);

static cl::opt<std::string>
optRPath("rpath", cl::desc("runtime path"), cl::ZeroOrMore);


// -----------------------------------------------------------------------------
static bool IsLLIRObject(llvm::StringRef buffer)
{
  return ReadData<uint32_t>(buffer, 0) == 0x52494C4C;
}

// -----------------------------------------------------------------------------
static bool IsLLARArchive(llvm::StringRef buffer)
{
  return ReadData<uint32_t>(buffer, 0) ==  0x52414C4C;
}

// -----------------------------------------------------------------------------
class Linker {
public:
  Linker(const char *argv0)
    : argv0_(argv0)
    , id_(0)
  {
  }

  /// Links a program.
  std::unique_ptr<Prog> LinkEXE(
      std::string_view output,
      std::vector<std::string> &missingLibs,
      std::set<std::string_view> entries)
  {
    // Preprocess all inputs.
    if (!LoadModules()) {
      return nullptr;
    }
    if (!LoadLibraries()) {
      return nullptr;
    }
    if (!FindDefinitions(entries)) {
      return nullptr;
    }

    // Build the program, starting with the entry point. Transfer relevant
    // symbols to the final program, recursively satisfying definitions.
    auto prog = std::make_unique<Prog>(output);
    for (std::string_view entry : entries) {
      if (auto *g = prog->GetGlobal(entry)) {
        continue;
      }
      auto it = defs_.find(std::string(entry));
      if (it != defs_.end()) {
        Transfer(&*prog, it->second);
      }
    }

    if (optStatic && (missingObjs_.empty() && missingLibs_.empty())) {
      ZeroWeakSymbols(prog.get());
    }

    if (!optExportDynamic) {
      for (Func &func : *prog) {
        if (entries.count(func.GetName()) == 0) {
          func.SetVisibility(Visibility::HIDDEN);
        } else {
          func.SetVisibility(Visibility::EXTERN);
        }
      }
      for (Data &data : prog->data()) {
        for (Object &object : data) {
          for (Atom &atom : object) {
            if (entries.count(atom.GetName()) == 0) {
              atom.SetVisibility(Visibility::HIDDEN);
            } else {
              atom.SetVisibility(Visibility::EXTERN);
            }
          }
        }
      }
    } else {
      std::set<Func *> export_fn;
      for (auto &mod : modules_) {
        for (Func &func : *mod) {
          if (func.IsExported()) {
            func.SetVisibility(Visibility::EXTERN);
            export_fn.insert(&func);
          }
        }
      }
      std::set<Object *> export_object;
      for (auto &mod : modules_) {
        for (Data &data : mod->data()) {
          for (Object &object : data) {
            for (Atom &atom : object) {
              if (atom.IsExported()) {
                export_object.insert(&object);
                break;
              }
            }
          }
        }
      }
      for (Func *func : export_fn) {
        Transfer(&*prog, func);
      }
      for (Object *object : export_object) {
        Transfer(&*prog, object);
      }
    }

    // Report the set of missing libraries.
    for (const std::string &obj : missingObjs_) {
      missingLibs.push_back(obj);
    }
    for (const std::string &lib : missingLibs_) {
      missingLibs.push_back(lib);
    }
    return prog;
  }

  /// Merges modules into a program.
  std::unique_ptr<Prog> Merge(std::string_view output)
  {
    // Preprocess all inputs.
    if (!LoadModules()) {
      return nullptr;
    }
    if (!LoadLibraries()) {
      return nullptr;
    }

    // Merge all modules into a single program.
    auto prog = std::make_unique<Prog>(output);
    for (unsigned i = 0, n = modules_.size(); i < n; ++i) {
      Prog *m = modules_[i].get();
      for (auto it = m->begin(), end = m->end(); it != end; ) {
        Func *func = &*it++;
        func->removeFromParent();
        prog->AddFunc(func);
      }
      for (auto it = m->data_begin(), end = m->data_end(); it != end; ) {
        Data *data = &*it++;
        if (Data *prev = prog->GetData(data->GetName())) {
          for (auto it = data->begin(); it != data->end(); ) {
            Object *object = &*it++;
            object->removeFromParent();
            prev->AddObject(object);
          }
          data->eraseFromParent();
        } else {
          data->removeFromParent();
          prog->AddData(data);
        }
      }
      for (auto it = m->ext_begin(), end = m->ext_end(); it != end; ) {
        Extern *ext = &*it++;
        if (auto *g = prog->GetGlobal(ext->GetName())) {
          ext->replaceAllUsesWith(g);
          ext->eraseFromParent();
        } else {
          ext->removeFromParent();
          prog->AddExtern(ext);
        }
      }
    }

    return prog;
  }

private:
  /// Load all modules.
  bool LoadModules();
  /// Load all libraries.
  bool LoadLibraries();
  /// Finds all definition sites.
  bool FindDefinitions(const std::set<std::string_view> &entries);

  /// Loads a single library.
  bool LoadLibrary(StringRef path);
  /// Loads an archive or an object file.
  bool LoadArchiveOrObject(StringRef path);
  /// Reads a LLIR library from a buffer.
  bool LoadArchive(StringRef path, llvm::StringRef buffer);
  /// Reads a LLIR object from a buffer.
  bool LoadObject(StringRef path, llvm::StringRef buffer);

  /// Records the definition site of a symbol.
  bool DefineSymbol(Global *g)
  {
    if (g->IsHidden()) {
      return true;
    }

    // If there are no prior definitions, record this one.
    auto it = defs_.emplace(std::string(g->GetName()), g);
    if (it.second) {
      return true;
    }

    // Allow strong symbols to override weak ones.
    if (it.first->second->IsWeak()) {
      it.first->second = g;
      return true;
    }

    WithColor::error(llvm::errs(), argv0_)
        << "duplicate symbol: " << g->getName() << "\n";
    return false;
  };

  /// Finds a library.
  std::optional<std::string> FindLibrary(StringRef library);

  /// Transfer a function to the program.
  void Transfer(Prog *prog, Func *g);
  /// Transfer a data item to the program.
  void Transfer(Prog *prog, Object *d);
  /// Transfer a global to the program.
  void Transfer(Prog *prog, Global *g);
  /// Transfer globals used by an expression.
  void Transfer(Prog *prog, Expr *e);

  /// Sets weak symbols to zero.
  void ZeroWeakSymbols(Prog *prog);

private:
  /// Name of the program, for diagnostics.
  const char *argv0_;
  /// List of loaded modules.
  std::vector<std::unique_ptr<Prog>> modules_;
  /// Map of definition sites.
  std::unordered_map<std::string, Global *> defs_;
  /// Map from names to aliases.
  std::unordered_map<std::string, Global *> aliases_;
  /// List of missing object files - provided in ELF form.
  std::vector<std::string> missingObjs_;
  /// List of missing archives - provided in ELF form.
  std::vector<std::string> missingLibs_;
  /// Next identifier for renaming.
  unsigned id_;
  /// Set of loaded modules.
  std::set<std::string> loaded_;
};

// -----------------------------------------------------------------------------
bool Linker::LoadModules()
{
  // Load all object files.
  for (StringRef path : optInput) {
    if (!LoadArchiveOrObject(path)) {
      return false;
    }
  }
  return true;
}

// -----------------------------------------------------------------------------
bool Linker::LoadLibraries()
{
  // Load all libraries.
  for (StringRef lib : optLibraries) {
    if (LoadLibrary(lib)) {
      continue;
    }

    WithColor::error(llvm::errs(), argv0_) << "missing lib: " << lib << "\n";
    return false;
  }
  return true;
}

// -----------------------------------------------------------------------------
bool Linker::LoadLibrary(StringRef path)
{
  for (StringRef libPath : optLibPaths) {
    if (!optStatic) {
      llvm::SmallString<128> pathSO(libPath);
      sys::path::append(pathSO, "lib" + path + ".so");
      if (sys::fs::exists(pathSO)) {
        return LoadArchiveOrObject(pathSO);
      }
    }

    llvm::SmallString<128> pathA(libPath);
    sys::path::append(pathA, "lib" + path + ".a");
    if (sys::fs::exists(pathA)) {
      return LoadArchiveOrObject(pathA);
    }
  }

  return false;
}

// -----------------------------------------------------------------------------
bool Linker::LoadArchiveOrObject(StringRef path)
{
  llvm::SmallString<256> fullPath = path;
  abspath(fullPath);
  auto FileOrErr = llvm::MemoryBuffer::getFile(fullPath);
  if (auto EC = FileOrErr.getError()) {
    WithColor::error(llvm::errs(), argv0_)
        << "cannot open " << path << ": " << EC.message() << "\n";
    return false;
  }

  auto buffer = FileOrErr.get()->getMemBufferRef().getBuffer();

  if (path.endswith(".a") || path.endswith(".so")) {
    if (IsLLARArchive(buffer)) {
      return LoadArchive(path, buffer);
    } else {
      missingLibs_.push_back(fullPath.str());
      return true;
    }
  }

  if (path.endswith(".o") || path.endswith(".lo") || path.endswith(".llbc")) {
    if (IsLLIRObject(buffer)) {
      return LoadObject(path, buffer);
    } else {
      missingLibs_.push_back(fullPath.str());
      return true;
    }
  }

  WithColor::error(llvm::errs(), argv0_) << "unknown format: " << path << "\n";
  return false;
}

// -----------------------------------------------------------------------------
bool Linker::LoadArchive(StringRef path, StringRef buffer)
{
  uint64_t count = ReadData<uint64_t>(buffer, sizeof(uint64_t));
  uint64_t meta = sizeof(uint64_t) + sizeof(uint64_t);
  for (unsigned i = 0; i < count; ++i) {
    size_t size = ReadData<uint64_t>(buffer, meta);
    meta += sizeof(uint64_t);
    uint64_t offset = ReadData<size_t>(buffer, meta);
    meta += sizeof(size_t);

    auto prog = BitcodeReader(StringRef(buffer.data() + offset, size)).Read();
    if (!prog) {
      return false;
    }
    if (!loaded_.insert(prog->GetName()).second) {
      return true;
    }
    modules_.push_back(std::move(prog));
  }
  return true;
}

// -----------------------------------------------------------------------------
bool Linker::LoadObject(StringRef path, StringRef buffer)
{
  auto prog = Parse(buffer, std::string_view(path.data(), path.size()));
  if (!prog) {
    return false;
  }
  if (!loaded_.insert(prog->GetName()).second) {
    return true;
  }
  modules_.push_back(std::move(prog));
  return true;
}

// -----------------------------------------------------------------------------
bool Linker::FindDefinitions(const std::set<std::string_view> &entries)
{
  // Find the set of external symbols required and provided by each module.
  std::unordered_map<Prog *, std::set<std::string>> needed;
  std::unordered_map<std::string, Prog *> providedBy;
  std::unordered_set<std::string> weaks;
  for (auto &module : modules_) {
    for (const Global *g : module->globals()) {
      std::string moduleName(g->GetName());
      if (const Extern *ext = ::dyn_cast_or_null<const Extern>(g)) {
        if (!ext->GetAlias()) {
          needed[module.get()].emplace(ext->GetName());
        } else {
          providedBy[moduleName] = module.get();
        }
        if (ext->IsWeak()) {
          weaks.emplace(ext->GetName());
        }
      } else {
        providedBy[moduleName] = module.get();
      }
    }
  }

  // Find the set of modules to consider, starting with entries.
  std::set<Prog *> modules;
  {
    std::queue<std::string> missing;
    for (std::string_view entry : entries) {
      missing.emplace(entry);
    }

    while (!missing.empty()) {
      std::string symbol = missing.front();
      missing.pop();

      auto it = providedBy.find(symbol);
      if (it == providedBy.end()) {
        if (optStatic && !weaks.count(symbol)) {
          WithColor::error(llvm::errs(), argv0_)
              << "undefined symbol \"" << symbol << "\", defaulting to 0x0\n";
          return false;
        }
        continue;
      }

      if (modules.insert(it->second).second) {
        for (const Extern &ext : it->second->externs()) {
          if (!ext.GetAlias()) {
            missing.emplace(ext.GetName());
          }
        }
      }
    }
  }

  // For each module considered, register names to globals.
  for (auto &module : modules) {
    for (Func &func : *module) {
      if (!DefineSymbol(&func)) {
        return false;
      }
    }

    for (Data &data : module->data()) {
      for (Object &object : data) {
        for (Atom &atom : object) {
          if (!DefineSymbol(&atom)) {
            return false;
          }
        }
      }
    }

    for (Extern &ext : module->externs()) {
      if (auto *alias = ext.GetAlias()) {
        aliases_[std::string(ext.GetName())] = alias;
      }
    }
  }
  return true;
}

// -----------------------------------------------------------------------------
void Linker::Transfer(Prog *p, Func *f)
{
  if (f->getParent() == p) {
    return;
  }
  f->removeFromParent();
  p->AddFunc(f);

  for (auto &block : *f) {
    for (auto &inst : block) {
      for (Value *v : inst.operand_values()) {
        switch (v->GetKind()) {
          case Value::Kind::INST:
          case Value::Kind::CONST: {
            continue;
          }
          case Value::Kind::GLOBAL: {
            Transfer(p, static_cast<Global *>(v));
            continue;
          }
          case Value::Kind::EXPR: {
            Transfer(p, static_cast<Expr *>(v));
            continue;
          }
        }
        llvm_unreachable("invalid value kind");
      }
    }
  }
}

// -----------------------------------------------------------------------------
std::optional<std::string> Linker::FindLibrary(StringRef library)
{
  if (sys::path::is_absolute(library)) {
    WithColor::error(llvm::errs(), argv0_)
        << "missing library: " << library << "\n";
    return {};
  }
  for (StringRef path : optLibPaths) {
    llvm_unreachable("not implemented");
  }
  return "";
}

// -----------------------------------------------------------------------------
void Linker::Transfer(Prog *p, Object *obj)
{
  Data *parent = obj->getParent();
  if (parent->getParent() == p) {
    return;
  }

  Data *data = p->GetOrCreateData(parent->GetName());
  obj->removeFromParent();
  data->AddObject(obj);

  for (auto &atom : *obj) {
    for (auto &item : atom) {
      if (item.GetKind() == Item::Kind::EXPR) {
        Transfer(p, item.GetExpr());
      }
    }
  }
}

// -----------------------------------------------------------------------------
void Linker::Transfer(Prog *p, Global *g)
{
  switch (g->GetKind()) {
    case Global::Kind::EXTERN: {
      auto *ext = static_cast<Extern *>(g);
      if (ext->getParent() == p) {
        return;
      }

      auto *existing = p->GetGlobal(ext->GetName());
      if (existing) {
        ext->replaceAllUsesWith(existing);
        ext->eraseFromParent();
        return;
      }

      std::string name(ext->GetName());
      auto dt = defs_.find(name);
      if (dt == defs_.end()) {
        auto at = aliases_.find(name);
        if (at != aliases_.end()) {
          ext->replaceAllUsesWith(at->second);
          ext->eraseFromParent();
          Transfer(p, at->second);
        } else {
          ext->removeFromParent();
          p->AddExtern(ext);
        }
      } else {
        ext->replaceAllUsesWith(dt->second);
        ext->eraseFromParent();
        Transfer(p, dt->second);
      }
      return;
    }
    case Global::Kind::FUNC: {
      auto *func = static_cast<Func *>(g);
      Transfer(p, func);
      return;
    }
    case Global::Kind::BLOCK: {
      auto *block = static_cast<Block *>(g);
      Transfer(p, block->getParent());
      return;
    }
    case Global::Kind::ATOM: {
      auto *atom = static_cast<Atom *>(g);
      Transfer(p, atom->getParent());
      return;
    }
  }
  llvm_unreachable("invalid global kind");
}

// -----------------------------------------------------------------------------
void Linker::Transfer(Prog *p, Expr *e)
{
  switch (e->GetKind()) {
    case Expr::Kind::SYMBOL_OFFSET: {
      auto *symOff = static_cast<SymbolOffsetExpr *>(e);
      Transfer(p, symOff->GetSymbol());
      return;
    }
  }
  llvm_unreachable("invalid expression kind");
}

// -----------------------------------------------------------------------------
void Linker::ZeroWeakSymbols(Prog *prog)
{
  for (auto it = prog->ext_begin(); it != prog->ext_end(); ) {
    Extern *ext = &*it++;
    for (auto it = ext->use_begin(); it != ext->use_end(); ) {
      Use &use = *it++;
      if (auto *inst = ::dyn_cast_or_null<Inst>(use.getUser())) {
        if (auto *movInst = ::dyn_cast_or_null<MovInst>(inst)) {
          use = new ConstantInt(0);
        } else {
          llvm_unreachable("invalid instruction");
        }
      } else {
        llvm_unreachable("not implemented");
      }
    }
  }
}

// -----------------------------------------------------------------------------
int WithTemp(char *argv0, StringRef ext, std::function<int(int, StringRef)> &&f)
{
  // Write the program to a bitcode file.
  auto tmp = llvm::sys::fs::TempFile::create("/tmp/llir-ld-%%%%%%%" + ext);
  if (!tmp) {
    WithColor::error(llvm::errs(), argv0)
        << "cannot create temporary file: " << tmp.takeError();
    return EXIT_FAILURE;
  }

  // Run the program on the temp file.
  int code = f(tmp->FD, tmp->TmpName);

  // Delete the temp file.
  if (auto error = code ? tmp->keep() : tmp->discard()) {
    WithColor::error(llvm::errs(), argv0)
        << "cannot delete temporary file: " << error;
    return EXIT_FAILURE;
  }
  return code;
}

// -----------------------------------------------------------------------------
static int RunExecutable(
    const char *argv0,
    const char *exe,
    llvm::ArrayRef<StringRef> args)
{
  if (auto P = llvm::sys::findProgramByName(exe)) {
    if (auto code = llvm::sys::ExecuteAndWait(*P, args)) {
      auto &log = WithColor::error(llvm::errs(), argv0);
      log << "command failed: " << exe << " ";
      for (size_t i = 1, n = args.size(); i < n; ++i) {
        log << args[i] << " ";
      }
      log << "\n";
      return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
  }
  WithColor::error(llvm::errs(), argv0) << "missing executable: " << exe;
  return EXIT_FAILURE;
}

// -----------------------------------------------------------------------------
static int RunOpt(
    const char *argv0,
    StringRef input,
    StringRef output,
    bool shared)
{
  std::vector<StringRef> args;
  args.push_back("llir-opt");
  switch (optOptLevel) {
    case OptLevel::O0: args.push_back("-O0"); break;
    case OptLevel::O1: args.push_back("-O1"); break;
    case OptLevel::O2: args.push_back("-O2"); break;
    case OptLevel::O3: args.push_back("-O3"); break;
  }
  args.push_back("-o");
  args.push_back(output);
  args.push_back(input);
  if (shared) {
    args.push_back("-shared");
  }
  return RunExecutable(argv0, "llir-opt", args);
}

// -----------------------------------------------------------------------------
static bool DumpIR(char *argv0, Prog *prog) {
  // Dump the IR blob into a folder specified by an env var.
  if (auto *path = getenv("LLIR_LD_DUMP_LLBC")) {
    // Generate a file name.
    llvm::SmallString<128> filename(optOutput);
    abspath(filename);
    std::replace(filename.begin(), filename.end(), '/', '_');
    llvm::SmallString<128> llbcPath(path);
    sys::path::append(llbcPath, filename + ".llbc");

    // Write the llbc output.
    std::error_code err;
    auto output = std::make_unique<llvm::ToolOutputFile>(
        llbcPath,
        err,
        sys::fs::F_None
    );
    if (err) {
      WithColor::error(llvm::errs(), argv0) << err.message();
      return false;
    }

    BitcodeWriter(output->os()).Write(*prog);
    output->keep();
  }

  return true;
}

// -----------------------------------------------------------------------------
static int LinkShared(char *argv0, StringRef out)
{
  auto prog = Linker(argv0).Merge(std::string_view(out.data(), out.size()));
  if (!prog) {
    return EXIT_FAILURE;
  }

  if (out.endswith(".llir")) {
    std::error_code err;
    auto output = std::make_unique<llvm::ToolOutputFile>(
      out,
      err,
      sys::fs::F_None
    );
    if (err) {
      WithColor::error(llvm::errs(), argv0) << err.message();
      return EXIT_FAILURE;
    }
    Printer(output->os()).Print(*prog);
    output->keep();
    return EXIT_SUCCESS;
  } else if (out.endswith(".llbc")) {
    std::error_code err;
    auto output = std::make_unique<llvm::ToolOutputFile>(
      out,
      err,
      sys::fs::F_None
    );
    if (err) {
      WithColor::error(llvm::errs(), argv0) << err.message();
      return EXIT_FAILURE;
    }
    BitcodeWriter(output->os()).Write(*prog);
    output->keep();
    return EXIT_SUCCESS;
  } else {
    return WithTemp(argv0, ".llbc", [&](int fd, StringRef llirPath) {
      {
        llvm::raw_fd_ostream os(fd, false);
        BitcodeWriter(os).Write(*prog);
      }

      if (!DumpIR(argv0, prog.get())) {
        return EXIT_FAILURE;
      }

      if (out.endswith(".S") || out.endswith(".s")) {
        return RunOpt(argv0, llirPath, out, true);
      } else {
        return WithTemp(argv0, ".o", [&](int, StringRef elfPath) {
          if (auto code = RunOpt(argv0, llirPath, elfPath, true)) {
            return code;
          }

          std::vector<StringRef> args;
          args.push_back("ld");
          args.push_back(elfPath);
          args.push_back("-o");
          args.push_back(optOutput);
          args.push_back("-shared");
          return RunExecutable(argv0, "ld", args);
        });
      }
    });
  }
}

// -----------------------------------------------------------------------------
static int LinkRelocatable(char *argv0, StringRef out)
{
  auto prog = Linker(argv0).Merge(std::string_view(out.data(), out.size()));
  if (!prog) {
    return EXIT_FAILURE;
  }
  std::error_code err;
  auto output = std::make_unique<llvm::ToolOutputFile>(
      out,
      err,
      sys::fs::F_None
  );
  if (err) {
    return EXIT_FAILURE;
  }

  BitcodeWriter(output->os()).Write(*prog);

  output->keep();
  return EXIT_SUCCESS;
}

// -----------------------------------------------------------------------------
static int LinkEXE(char *argv0, StringRef out)
{
  OutputType type;
  if (out.endswith(".S") || out.endswith(".s")) {
    type = OutputType::ASM;
  } else if (out.endswith(".o")) {
    type = OutputType::OBJ;
  } else if (out.endswith(".llir")) {
    type = OutputType::LLIR;
  } else if (out.endswith(".llbc")) {
    type = OutputType::LLBC;
  } else {
    type = OutputType::EXE;
  }

  // Link the objects together.
  std::set<std::string_view> entries;
  entries.insert(optEntry);
  entries.insert("caml_garbage_collection");

  std::vector<std::string> missingLibs;
  auto prog = Linker(argv0).LinkEXE(
      std::string_view{out.data(), out.size()},
      missingLibs,
      entries
  );
  if (!prog) {
    return EXIT_FAILURE;
  }

  if (type == OutputType::LLIR) {
    return WithTemp(argv0, ".llir", [&](int fd, StringRef llirPath) {
      {
        llvm::raw_fd_ostream os(fd, false);
        BitcodeWriter(os).Write(*prog);
      }
      return RunOpt(argv0, llirPath, out, false);
    });
  } else if (type == OutputType::LLBC) {
    return WithTemp(argv0, ".llbc", [&](int fd, StringRef llirPath) {
      {
        llvm::raw_fd_ostream os(fd, false);
        BitcodeWriter(os).Write(*prog);
      }
      return RunOpt(argv0, llirPath, out, false);
    });
  } else {
    return WithTemp(argv0, ".llbc", [&](int fd, StringRef llirPath) {
      {
        llvm::raw_fd_ostream os(fd, false);
        BitcodeWriter(os).Write(*prog);
      }

      if (!DumpIR(argv0, prog.get())) {
        return EXIT_FAILURE;
      }

      if (type == OutputType::OBJ || type == OutputType::ASM) {
        return RunOpt(argv0, llirPath, out, false);
      } else {
        return WithTemp(argv0, ".o", [&](int, StringRef elfPath) {
          if (auto code = RunOpt(argv0, llirPath, elfPath, false)) {
            return code;
          }

          std::vector<StringRef> args;
          args.push_back("ld");
          args.push_back("-nostdlib");
          args.push_back("--start-group");
          args.push_back(elfPath);
          for (StringRef lib : missingLibs) {
            args.push_back(lib);
          }
          args.push_back("--end-group");
          args.push_back("-o");
          args.push_back(optOutput);
          if (!optDynamicLinker.empty()) {
            args.push_back("-dynamic-linker");
            args.push_back(optDynamicLinker);
          }
          if (optExportDynamic) {
            args.push_back("-E");
          }
          if (optStatic) {
            args.push_back("-static");
          }
          return RunExecutable(argv0, "ld", args);
        });
      }
    });
  }
}


// -----------------------------------------------------------------------------
int main(int argc, char **argv)
{
  llvm::InitLLVM X(argc, argv);

  // Parse command line options.
  if (!llvm::cl::ParseCommandLineOptions(argc, argv, "LLIR optimiser\n\n")) {
    return EXIT_FAILURE;
  }

  // Canonicalise the file name.
  llvm::SmallString<256> output{llvm::StringRef(optOutput)};
  abspath(output);

  // Emit the output in the desired format.
  if (optShared) {
    return LinkShared(argv[0], output);
  } else if (optRelocatable) {
    return LinkRelocatable(argv[0], output);
  } else {
    return LinkEXE(argv[0], output);
  }
}
