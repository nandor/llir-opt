// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <set>
#include <unordered_map>

#include <llvm/Support/WithColor.h>

class Prog;



/**
 * Helper class to link object files and executables.
 */
class Linker {
public:
  /// Initialise the linker.
  Linker(const char *argv0)
    : argv0_(argv0)
    , id_(0)
  {
  }

  /// Links a program.
  std::unique_ptr<Prog> Link(
      const std::vector<std::unique_ptr<Prog>> &objects,
      const std::vector<std::unique_ptr<Prog>> &archives,
      std::string_view output,
      const std::set<std::string> &entries
  );

private:
  /// Finds all definition sites.
  bool FindDefinitions(std::set<std::string> &entries);
  /// Records the definition site of a symbol.
  bool DefineSymbol(Global *g);
  /// Sets weak symbols to zero.
  void ZeroWeakSymbols(Prog *prog);

private:
  /// Name of the program, for diagnostics.
  const char *argv0_;
  /// Map of definition sites.
  std::unordered_map<std::string, Global *> defs_;
  /// Map from names to aliases.
  std::unordered_map<std::string, Global *> aliases_;
  /// Next identifier for renaming.
  unsigned id_;
  /// Set of loaded modules.
  std::set<std::string> loaded_;
};
