// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <ostream>
#include <unordered_map>

#include <llvm/Support/raw_ostream.h>

#include "core/calling_conv.h"
#include "core/visibility.h"
#include "core/inst.h"

class Atom;
class Block;
class Data;
class Func;
class Prog;
class Object;



/**
 * Prints a program.
 */
class Printer {
public:
  /// Initialises the printer.
  Printer(llvm::raw_ostream &os) : os_(os) {}

  /// Prints a whole program.
  void Print(const Prog &prog);
  /// Prints a data segment.
  void Print(const Data &data);
  /// Prints an object.
  void Print(const Object &object);
  /// Prints an atom.
  void Print(const Atom &atom);
  /// Prints a function.
  void Print(const Func &func);
  /// Prints a block.
  void Print(const Block &block);
  /// Prints an instruction.
  void Print(const Inst &inst);
  /// Prints an expression.
  void Print(const Expr &expr);
  /// Prints a value.
  void Print(ConstRef<Value> val);
  /// Prints a type.
  void Print(Type type);
  /// Print a type flag.
  void Print(TypeFlag type);
  /// Prints a flagged type.
  void Print(FlaggedType type);
  /// Print a calling convention.
  void Print(CallingConv conv);
  /// Print a calling convention.
  void Print(Visibility visibility);
  /// Print a quoted string.
  void Print(const std::string_view str);

private:
  /// Output stream.
  llvm::raw_ostream &os_;
  /// Instruction to identifier map.
  std::unordered_map<ConstRef<Inst>, unsigned> insts_;
};
