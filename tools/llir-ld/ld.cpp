// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <sstream>
#include <set>
#include <unordered_set>

#include <llvm/ADT/PointerUnion.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/Endian.h>
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
namespace endian = llvm::support::endian;
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
optRelocatable("r", cl::desc("relocatable"));

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

// -----------------------------------------------------------------------------
template <typename Magic, Magic M>
bool CheckMagic(llvm::StringRef buffer)
{
  if (buffer.size() < sizeof(M)) {
    return false;
  }

  auto *buf = buffer.data();
  auto magic = endian::read<uint32_t, llvm::support::little, 1>(buf);
  return magic == M;
}


// -----------------------------------------------------------------------------
static bool IsElfObject(llvm::StringRef buffer)
{
  return CheckMagic<uint32_t, 0x464C457F>(buffer);
}

// -----------------------------------------------------------------------------
static bool IsLLARArchive(llvm::StringRef buffer)
{
  return CheckMagic<uint32_t,  0x52414C4C>(buffer);
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
    if (!FindDefinitions()) {
      return nullptr;
    }

    // Build the program, starting with the entry point. Transfer relevant
    // symbols to the final program, recursively satisfying definitions.
    auto prog = std::make_unique<Prog>();
    for (std::string_view entry : entries) {
      if (auto *g = prog->GetGlobal(entry)) {
        continue;
      }
      auto it = defs_.find(std::string(entry));
      if (it != defs_.end()) {
        Transfer(&*prog, it->second);
      }
    }

    for (Func &func : *prog) {
      if (entries.count(func.GetName()) == 0) {
        func.SetVisibility(Visibility::HIDDEN);
      } else {
        func.SetVisibility(Visibility::EXTERN);
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
  std::unique_ptr<Prog> Merge()
  {
    // Preprocess all inputs.
    if (!LoadModules()) {
      return nullptr;
    }
    if (!LoadLibraries()) {
      return nullptr;
    }

    // Merge all modules into the first one.
    if (modules_.empty()) {
      return std::make_unique<Prog>();
    }

    auto prog = std::move(modules_[0]);
    for (unsigned i = 1, n = modules_.size(); i < n; ++i) {
      Prog *m = modules_[i].get();
      for (auto it = m->begin(), end = m->end(); it != end; ) {
        Func *func = &*it++;
        func->removeFromParent();
        prog->AddFunc(func);
      }
      for (auto it = m->data_begin(), end = m->data_end(); it != end; ) {
        Data *data = &*it++;
        data->removeFromParent();
        prog->AddData(data);
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
  bool FindDefinitions();

  /// Loads a single library.
  bool LoadLibrary(StringRef path);
  /// Loads an archive or an object file.
  bool LoadArchiveOrObject(StringRef path);
  /// Reads a LLIR library from a buffer.
  bool LoadArchive(llvm::StringRef buffer);

  /// Records the definition site of a symbol.
  void DefineSymbol(Global *g)
  {
    if (g->IsHidden()) {
      return;
    }

    // If there are no prior definitions, record this one.
    auto it = defs_.emplace(std::string(g->GetName()), g);
    if (it.second) {
      return;
    }

    // Allow strong symbols to override weak ones.
    if (it.first->second->IsWeak()) {
      it.first->second = g;
      return;
    }
    llvm::report_fatal_error("duplicate symbol");
  };

  /// Finds a library.
  std::optional<std::string> FindLibrary(StringRef library);

  /// Transfer a function to the program.
  void Transfer(Prog *prog, Func *g);
  /// Transfer a data item to the program.
  void Transfer(Prog *prog, Data *d);
  /// Transfer a global to the program.
  void Transfer(Prog *prog, Global *g);
  /// Transfer globals used by an expression.
  void Transfer(Prog *prog, Expr *e);

private:
  /// Name of the program, for diagnostics.
  const char *argv0_;
  /// List of loaded modules.
  std::vector<std::unique_ptr<Prog>> modules_;
  /// Map of definition sites.
  std::unordered_map<std::string, Global *> defs_;
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
    llvm::SmallString<128> fullPath(libPath);
    sys::path::append(fullPath, "lib" + path + ".a");
    if (!sys::fs::exists(fullPath)) {
      continue;
    }
    return LoadArchiveOrObject(fullPath);
  }

  return false;
}

// -----------------------------------------------------------------------------
bool Linker::LoadArchiveOrObject(StringRef path)
{
  if (!loaded_.insert(path.str()).second) {
    return true;
  }

  llvm::SmallString<256> fullPath = path;
  sys::fs::make_absolute(fullPath);
  sys::path::remove_dots(fullPath);
  auto FileOrErr = llvm::MemoryBuffer::getFile(fullPath);
  if (auto EC = FileOrErr.getError()) {
    WithColor::error(llvm::errs(), argv0_)
        << "cannot open: " << EC.message() << "\n";
    return false;
  }

  auto buffer = FileOrErr.get()->getMemBufferRef().getBuffer();

  if (path.endswith(".a")) {
    if (IsLLARArchive(buffer)) {
      return LoadArchive(buffer);
    } else {
      missingLibs_.push_back(fullPath.str());
      return true;
    }
  }

  if (path.endswith(".o") || path.endswith(".lo") || path.endswith(".llbc")) {
    if (IsElfObject(buffer)) {
      missingLibs_.push_back(fullPath.str());
      return true;
    }

    auto prog = Parse(buffer);
    if (!prog) {
      return false;
    }
    modules_.push_back(std::move(prog));
    return true;
  }

  WithColor::error(llvm::errs(), argv0_) << "unknown format: " << path << "\n";
  return false;
}

// -----------------------------------------------------------------------------
template<typename T> T ReadData(StringRef buffer, uint64_t offset)
{
  if (offset + sizeof(T) > buffer.size()) {
    llvm::report_fatal_error("invalid bitcode file");
  }

  auto *data = buffer.data() + offset;
  return endian::read<T, llvm::support::little, 1>(data);
}

// -----------------------------------------------------------------------------
bool Linker::LoadArchive(llvm::StringRef buffer)
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
    modules_.push_back(std::move(prog));
  }
  return true;
}

// -----------------------------------------------------------------------------
bool Linker::FindDefinitions()
{
  for (auto &module : modules_) {
    for (Func &func : *module) {
      DefineSymbol(&func);
    }

    for (Data &data : module->data()) {
      for (Atom &atom : data) {
        DefineSymbol(&atom);
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
void Linker::Transfer(Prog *p, Data *d)
{
  if (d->getParent() == p) {
    return;
  }

  if (auto *prev = p->GetData(d->GetName())) {
    // Concatenate segments. Add a delimiter to the end of the previous block.
    if (!prev->empty()) {
      prev->rbegin()->AddEnd();
    }

    std::vector<Expr *> exprs;
    for (auto it = d->begin(); it != d->end(); ) {
      Atom *atom = &*it++;
      atom->removeFromParent();
      prev->AddAtom(atom);
      for (auto &item : *atom) {
        if (item.GetKind() == Item::Kind::EXPR) {
          exprs.push_back(item.GetExpr());
        }
      }
    }
    d->eraseFromParent();
    for (auto &expr : exprs) {
      Transfer(p, expr);
    }
  } else {
    // Add the new segment to the program.
    d->removeFromParent();
    p->AddData(d);

    for (auto &atom : *d) {
      for (auto &item : atom) {
        if (item.GetKind() == Item::Kind::EXPR) {
          Transfer(p, item.GetExpr());
        }
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

      auto it = defs_.find(std::string(ext->GetName()));
      if (it == defs_.end()) {
        ext->removeFromParent();
        p->AddExtern(ext);
      } else {
        ext->replaceAllUsesWith(it->second);
        ext->eraseFromParent();
        Transfer(p, it->second);
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
static int RunOpt(const char *argv0, StringRef input, StringRef output)
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
  return RunExecutable(argv0, "llir-opt", args);
}

// -----------------------------------------------------------------------------
static int RunLD(
    const char *argv0,
    const std::vector<std::string> &libs,
    StringRef input,
    StringRef output)
{
  std::vector<StringRef> args;
  args.push_back("ld");
  args.push_back("--start-group");
  args.push_back(input);
  for (auto &lib : libs) {
    args.push_back(lib);
  }
  args.push_back("--end-group");
  args.push_back("-o");
  args.push_back(output);
  args.push_back("-static");
  return RunExecutable(argv0, "ld", args);
}

// -----------------------------------------------------------------------------
int main(int argc, char **argv)
{
  llvm::InitLLVM X(argc, argv);

  // Parse command line options.
  if (!llvm::cl::ParseCommandLineOptions(argc, argv, "LLIR optimiser\n\n")) {
    return EXIT_FAILURE;
  }

  // Determine the output type.
  StringRef out(optOutput);
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

  // Emit the output in the desired format.
  if (optRelocatable) {
    auto prog = Linker(argv[0]).Merge();
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
  } else {
     // Link the objects together.
    std::set<std::string_view> entries;
    entries.insert(optEntry);
    entries.insert("caml_garbage_collection");

    std::vector<std::string> missingLibs;
    auto prog = Linker(argv[0]).LinkEXE(missingLibs, entries);
    if (!prog) {
      return EXIT_FAILURE;
    }

    if (type == OutputType::LLIR) {
      std::error_code err;
      auto output = std::make_unique<llvm::ToolOutputFile>(
          out,
          err,
          sys::fs::F_Text
      );
      if (err) {
        WithColor::error(llvm::errs(), argv[0]) << err.message();
        return EXIT_FAILURE;
      }
      Printer(output->os()).Print(*prog);
      output->keep();
      return EXIT_SUCCESS;

    } else if (type == OutputType::LLBC) {
      std::error_code err;
      auto output = std::make_unique<llvm::ToolOutputFile>(
        out,
        err,
        sys::fs::F_None
      );
      if (err) {
        WithColor::error(llvm::errs(), argv[0]) << err.message();
        return EXIT_FAILURE;
      }
      BitcodeWriter(output->os()).Write(*prog);
      output->keep();
      return EXIT_SUCCESS;
    } else {
      return WithTemp(argv[0], ".llbc", [&](int fd, StringRef llirPath) {
        {
          llvm::raw_fd_ostream os(fd, false);
          BitcodeWriter(os).Write(*prog);
        }

        if (type == OutputType::OBJ || type == OutputType::ASM) {
          return RunOpt(argv[0], llirPath, out);
        } else {
          return WithTemp(argv[0], ".o", [&](int, StringRef elfPath) {
            if (auto code = RunOpt(argv[0], llirPath, elfPath)) {
              return code;
            }
            return RunLD(argv[0], missingLibs, elfPath, optOutput);
          });
        }
      });
    }
  }
}
