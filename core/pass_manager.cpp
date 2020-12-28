// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <chrono>
#include <iostream>

#include <llvm/Support/Format.h>

#include "core/pass.h"
#include "core/pass_manager.h"
#include "core/printer.h"



// -----------------------------------------------------------------------------
PassManager::PassManager(const PassConfig &config, bool verbose, bool time)
  : config_(config)
  , verbose_(verbose)
  , time_(time)
{
}

// -----------------------------------------------------------------------------
void PassManager::Run(Prog &prog)
{
  if (verbose_) {
    llvm::outs() << "\n--- Initial code:\n\n";
    Printer(llvm::outs()).Print(prog);
  }

  for (auto &group : groups_) {
    bool changed;
    do {
      changed = false;
      for (auto &pass : group.Passes) {
        if (Run(pass, prog)) {
          changed = true;
          analyses_.clear();
        }
      }
    } while (group.Repeat && changed);
  }

  if (verbose_) {
    llvm::outs() << "\n--- Done\n\n";
  }
}

// -----------------------------------------------------------------------------
bool PassManager::Run(PassInfo &pass, Prog &prog)
{
  const auto &name = pass.P->GetPassName();

  // Print information.
  if (time_) {
    llvm::outs() << name << ": ";
  }

  // Run the pass, measuring elapsed time.
  double elapsed;
  bool changed;
  {
    const auto start = std::chrono::high_resolution_clock::now();
    changed = pass.P->Run(prog);
    const auto end = std::chrono::high_resolution_clock::now();

    elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        end - start
    ).count() / 1e6;
  }

  // If verbose, print IR after pass.
  if (verbose_) {
    llvm::outs() <<"\n--- " << pass.P->GetPassName() << "\n\n";
    Printer(llvm::outs()).Print(prog);
  }

  // If timed, print duration.
  if (time_) {
    llvm::outs() << llvm::format("%.5f", elapsed) << "s";
    if (changed) {
      llvm::outs() << ", changed";
    }
    llvm::outs() << "\n";
  }

  // Record the analysis results.
  if (pass.ID) {
    analyses_.emplace(pass.ID, pass.P.get());
  }

  return changed;
}
