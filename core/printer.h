// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <ostream>

class Prog;
class Func;



/**
 * Prints a program.
 */
class Printer {
public:
  /// Initialises the printer.
  Printer(std::ostream &os) : os_(os)
  {
  }

  /// Prints a whole program.
  void Print(const Prog *prog);
  /// Prints a function.
  void Print(const Func *func);

private:
  /// Output stream.
  std::ostream &os_;
};
