// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <iostream>

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
    std::cout << "\n--- Initial code:\n\n";
    Printer(llvm::outs()).Print(prog);
  }
  for (auto *pass : passes_) {
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
      std::cout <<"\n--- " << pass->GetPassName() << "\n\n";
      Printer(llvm::outs()).Print(prog);
    }

    // If timed, print duration.
    if (time_) {
      std::cout << pass->GetPassName() << ": " << elapsed << "s\n";
    }
  }
  if (verbose_) {
    std::cout << "\n--- Done\n\n";
  }
}
