// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <llvm/Support/raw_ostream.h>

#include "core/block.h"

class Prog;
class Func;



/**
 * Coq IR emitter
 */
class CoqEmitter final {
public:
  /// Creates a coq emitter.
  CoqEmitter(llvm::raw_ostream &os);

  /// Writes a program.
  void Write(const Prog &prog);

private:
  /// Writes a function.
  void WriteDefinition(const Func &func);
  /// Writes an instruction.
  void Write(Block::const_iterator it);
  /// Writes a unary instruction.
  void Unary(Block::const_iterator it, const char *op);
  /// Writes a binary instruction.
  void Binary(Block::const_iterator it, const char *op);
  /// Writes a mov instruction.
  void Mov(Block::const_iterator it);
  /// Write an integer.
  template<unsigned Bits>
  void MovInt(Block::const_iterator it, const char *op, const APInt &val);

  /// Writes an inversion theorem.
  void WriteInversion(const Func &func);
  /// Writes an inversion theorem for definitions.
  void WriteDefinedAtInversion(const Func &func);
  /// Writes an inversion theorem for uses.
  void WriteUsedAtInversion(const Func &func);
  /// Writes basic blocks.
  void WriteBlocks(const Func &func);
  /// Writes dominators.
  void WriteDominators(const Func &func);

  /// Writes a proof of validity.
  void WriteUsesHaveDefs(const Func &func);
  /// Writes a proof of validity.
  void WriteDefsAreUniqe(const Func &func);
  /// Writes the well-typed proof.
  void WriteWellTyped(const Func &func);

  /// Writes a type.
  void Write(Type ty);

  /// Sanitised function name.
  std::string Name(const Func &func);

private:
  /// Mapping from instructions to IDs.
  std::unordered_map<const Inst *, unsigned> insts_;
  /// Mapping from blocks to IDs.
  std::unordered_map<const Block *, unsigned> blocks_;
  /// Stream to write to.
  llvm::raw_ostream &os_;
};
