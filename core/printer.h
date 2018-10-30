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
  Printer(std::ostream &os) : os_(os) {}

  /// Prints a whole program.
  void Print(const Prog *prog);
  /// Prints a function.
  void Print(const Func *func);
  /// Prints a block.
  void Print(const Block *block);
  /// Prints an instruction.
  void Print(const Inst *inst);
  /// Prints an operand.
  void Print(const Operand &op);

private:
  /// Output stream.
  std::ostream &os_;
  /// Instruction to identifier map.
  std::unordered_map<const Inst *, unsigned> insts_;
};
