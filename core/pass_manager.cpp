// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <iostream>

#include "core/pass.h"
#include "core/pass_manager.h"
#include "core/printer.h"



// -----------------------------------------------------------------------------
PassManager::PassManager(bool verbose)
  : verbose_(verbose)
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
    std::cout << std::endl << "--- Initial code:";
    std::cout << std::endl << std::endl;
    Printer(std::cout).Print(prog);
  }
  for (auto *pass : passes_) {
    pass->Run(prog);
    if (verbose_) {
      std::cout << std::endl << "--- " << pass->GetPassName();
      std::cout << std::endl << std::endl;
      Printer(std::cout).Print(prog);
    }
  }
}
