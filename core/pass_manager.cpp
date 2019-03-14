// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <iostream>

#include <llvm/Support/format.h>

#include "core/pass.h"
#include "core/pass_manager.h"
#include "core/printer.h"



// -----------------------------------------------------------------------------
PassManager::PassManager(bool verbose, bool time)
  : verbose_(verbose)
  , time_(time)
{
}

// -----------------------------------------------------------------------------
void PassManager::Add(Pass *pass)
{
  passes_.push_back(pass);
}

// -----------------------------------------------------------------------------
void PassManager::Run(Prog *prog)
{
  if (verbose_) {
    llvm::outs() << "\n--- Initial code:\n\n";
    Printer(llvm::outs()).Print(prog);
  }

  for (auto *pass : passes_) {
    const auto &name = pass->GetPassName();

    // Run the pass, measuring elapsed time.
    double elapsed;
    {
      const auto start = std::chrono::high_resolution_clock::now();
      pass->Run(prog);
      const auto end = std::chrono::high_resolution_clock::now();

      elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
          end - start
      ).count() / 1e6;
    }

    // If verbose, print IR after pass.
    if (verbose_) {
      llvm::outs() <<"\n--- " << pass->GetPassName() << "\n\n";
      Printer(llvm::outs()).Print(prog);
    }

    // If timed, print duration.
    if (time_) {
      llvm::outs() << name << ": " << llvm::format("%.5f", elapsed) << "s\n";
    }
  }
  if (verbose_) {
    llvm::outs() << "\n--- Done\n\n";
  }
}
