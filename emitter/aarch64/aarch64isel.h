// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <set>
#include <unordered_map>

#include <llvm/ADT/DenseMap.h>
#include <llvm/Analysis/OptimizationRemarkEmitter.h>
#include <llvm/CodeGen/MachineBasicBlock.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/Pass.h>
#include <llvm/Target/AArch64/AArch64InstrInfo.h>
#include <llvm/Target/AArch64/AArch64ISelDAGToDAG.h>
#include <llvm/Target/AArch64/AArch64MachineFunctionInfo.h>
#include <llvm/Target/AArch64/AArch64RegisterInfo.h>
#include <llvm/Target/AArch64/AArch64Subtarget.h>
#include <llvm/Target/AArch64/AArch64TargetMachine.h>

#include "core/insts.h"
#include "emitter/isel.h"
#include "emitter/aarch64/aarch64call.h"

class Data;
class Func;
class Inst;
class Prog;
