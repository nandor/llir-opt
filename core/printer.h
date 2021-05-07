// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <ostream>
#include <unordered_map>

#include <llvm/Support/raw_ostream.h>

#include "core/cond.h"
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
  virtual void Print(const Prog &prog);
  /// Prints a data segment.
  virtual void Print(const Data &data);
  /// Prints an object.
  virtual void Print(const Object &object);
  /// Prints an atom.
  virtual void Print(const Atom &atom);
  /// Prints a function.
  virtual void Print(const Func &func);
  /// Prints a block.
  virtual void Print(const Block &block);
  /// Prints an instruction.
  virtual void Print(const Inst &inst);
  /// Prints an expression.
  virtual void Print(const Expr &expr);
  /// Prints a value.
  virtual void Print(ConstRef<Value> val);
  /// Print a quoted string.
  virtual void Print(const std::string_view str);

protected:
  /// Hook to print additional information for functions.
  virtual void PrintFuncHeader(const Func &func) {}
  /// Hook to print additional information for instructions.
  virtual void PrintInstHeader(const Inst &inst) {}

private:
  /// Auto-generated printer implementation.
  void PrintImpl(const Inst &inst);

protected:
  /// Output stream.
  llvm::raw_ostream &os_;

private:
  /// Instruction to identifier map.
  std::unordered_map<ConstRef<Inst>, unsigned> insts_;
};
