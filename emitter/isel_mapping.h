// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <unordered_map>

#include <llvm/CodeGen/MachineFunction.h>
#include <llvm/MC/MCSymbol.h>

class Inst;
class Block;
class Func;



/**
 * Mapping from LLIR to LLVM objects.
 */
class ISelMapping {
public:
  /// Finds the MachineFunction attached to a function.
  llvm::MachineFunction *operator[] (const Func *func) const;
  /// Finds the label attached to an instruction.
  llvm::MCSymbol *operator[] (const Inst *inst) const;
  /// Finds the MachineBasicBlock attached to a block.
  llvm::MachineBasicBlock *operator[] (const Block *block) const;

protected:
  /// Mapping from functions to MachineFunctions.
  std::unordered_map<const Func *, llvm::MachineFunction *> funcs_;
  /// Mapping from blocks to machine blocks.
  std::unordered_map<const Block *, llvm::MachineBasicBlock *> blocks_;
  /// Labels of annotated instructions.
  std::unordered_map<const Inst *, llvm::MCSymbol *> labels_;
};
