// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <set>
#include <unordered_map>

#include <llvm/Support/WithColor.h>

class Prog;
class Func;
class Data;
class Xtor;
class Global;



/**
 * Helper class to link object files and executables.
 */
class Linker {
public:
  /// A unit to link.
  class Unit {
  public:
    /// Enumeration of objects to be linked.
    enum class Kind {
      /// LLIR program.
      LLIR,
      /// LLVM bitcode.
      BITCODE,
      /// Regular object file.
      OBJECT,
      /// Arbitrary data.
      DATA,
    };

    /// Create a unit for an LLIR program.
    Unit(std::unique_ptr<Prog> &&prog);

    /// Crete a unit for an LLVM bitcode object.
    struct Bitcode { llvm::StringRef Data; };
    Unit(const Bitcode &data);

    /// Create a unit for an ELF object.
    struct Object { llvm::StringRef Path; };
    Unit(const Object &object);

    /// Create a unit for an arbitrary data file.
    struct Data { llvm::StringRef Path; };
    Unit(const Data &data);

    /// Move constructor.
    Unit(Unit &&);
    /// Destructor.
    ~Unit();

    /// Move-assignment operator.
    Unit &operator=(Unit &&);

  private:
    friend class Linker;
    /// Kind of the unit.
    Kind kind_;
    /// Union of stored items.
    union S {
      std::unique_ptr<Prog> P;

      S() {}
      ~S() {}
    } s_;
  };

  /// Representation for an entire archive.
  using Archive = std::vector<Linker::Unit>;

  /// Initialise the linker.
  Linker(std::string_view output) : output_(output) {}

  /// Link an object, unconditionally.
  llvm::Error LinkObject(Unit &&unit);
  /// Link a group of units.
  llvm::Error LinkGroup(std::vector<Linker::Unit> &&units);

  /// Return the resulting program.
  using LinkResult = std::pair<std::unique_ptr<Prog>, std::vector<std::string>>;
  llvm::Expected<LinkResult> Link();

private:
  /// Run the LLVM LTO, resulting in a list of programs.
  std::vector<std::unique_ptr<Prog>> LTO();
  /// Resolve unresolved symbols
  void Resolve(Prog &prog);
  /// Resolve a global name.
  void Resolve(const std::string &name);

  /// Merge a module into the program.
  bool Merge(Prog &dest, Prog &source);
  /// Merge a function.
  bool Merge(Prog &dest, Func &func);
  /// Merge a data segment.
  bool Merge(Prog &dest, Data &data);
  /// Merge a constructor/destructor.
  bool Merge(Prog &dest, Xtor &xtor);

private:
  /// Name of the output.
  std::string output_;
  /// Set of object files to link.
  std::vector<Unit> units_;
  /// Set of linked-in external objects.
  std::vector<std::string> files_;
  /// Set of unresolved symbols.
  std::set<std::string> unresolved_;
  /// Set of resolved symbols.
  std::set<std::string> resolved_;
  /// Set of linked program IDs to avoid duplicates.
  std::set<llvm::StringRef> linked_;
};
