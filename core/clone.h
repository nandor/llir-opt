// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <unordered_map>

#include "core/cast.h"
#include "core/insts.h"

class Inst;
class Block;
class Global;
class Prog;



/**
 * Helper class which clones instructions.
 */
class CloneVisitor {
public:
  /// Destroys the visitor.
  virtual ~CloneVisitor();

public:
  /// Maps a block to a new one.
  virtual Block *Map(Block *block) { return block; }
  /// Maps a block to a new one.
  virtual Func *Map(Func *func) { return func; }
  /// Maps a block to a new one.
  virtual Extern *Map(Extern *ext) { return ext; }
  /// Maps an atom to a new one.
  virtual Atom *Map(Atom *atom) { return atom; }
  /// Maps a constant to a new one.
  virtual Constant *Map(Constant *constant) { return constant; }
  /// Maps a global to a potentially new one.
  virtual Global *Map(Global *global);
  /// Maps an expression to a potentially new one.
  virtual Expr *Map(Expr *expr);
  /// Clones an annotation.
  virtual AnnotSet Annot(const Inst *inst);

public:
  /// Maps an instruction reference to a new one.
  virtual Ref<Inst> Map(Ref<Inst> inst) { return inst; }

public:
  /// Clones an instruction.
  virtual Inst *Clone(Inst *inst);
  /// Fixes PHI nodes.
  void Fixup();

public:
  // Declaration of individual clone methods.
  #define GET_INST(kind, type, name, sort) \
    virtual Inst *Clone(type *i);
  #include "instructions.def"

protected:
  /// Maps a value to a potentially new one.
  Ref<Value> Map(Ref<Value> value);
  /// Maps a block to a potentially new one.
  Ref<Block> Map(Ref<Block> value);

  template<typename It, typename T = typename It::value_type>
  std::vector<T> Map(llvm::iterator_range<It> range)
  {
    std::vector<T> args;
    for (T arg : range) {
      args.push_back(Map(arg));
    }
    return args;
  }

protected:
  /// PHI instruction delayed fixups.
  llvm::SmallVector<std::pair<PhiInst *, PhiInst *>, 10> fixups_;
};


/**
 * Clone a program and return the copy of a specific instruction.
 */
std::pair<std::unique_ptr<Prog>, Inst *> Clone(Prog &oldProg, Inst *inst);


/**
 * Clone a program and return the copy of a specific instruction.
 */
template<typename T>
std::pair<std::unique_ptr<Prog>, T *> CloneT(Prog &oldProg, T *inst)
{
  auto &&[prog, newInst] = Clone(oldProg, inst);
  return { std::move(prog), static_cast<T *>(newInst) };
}


/**
 * Helper method to clone a program.
 */
std::unique_ptr<Prog> Clone(Prog &oldProg);
