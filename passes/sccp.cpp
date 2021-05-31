// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <set>
#include <queue>

#include <llvm/Support/Debug.h>
#include <llvm/ADT/Statistic.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/constant.h"
#include "core/func.h"
#include "core/prog.h"
#include "core/insts.h"
#include "core/pass_manager.h"
#include "passes/sccp.h"
#include "passes/sccp/lattice.h"
#include "passes/sccp/solver.h"

#define DEBUG_TYPE "sccp"

STATISTIC(NumConstantsFolded, "Number of constants folded by SCCP");


// -----------------------------------------------------------------------------
static bool Rewrite(Func &func, SCCPSolver &solver)
{
  LLVM_DEBUG(llvm::dbgs() << func.getName() << ":\n");
  bool changed = false;
  bool removeUnreachable = false;
  for (auto &block : func) {
    if (!solver.IsExecutable(block)) {
      LLVM_DEBUG(llvm::dbgs() << "\t" << block.getName() << ": unreachable\n");
      // If the block is not reachable, replace it with a trap.
      if (block.size() > 1 || !block.GetTerminator()->Is(Inst::Kind::TRAP)) {
        std::set<Block *> succs(block.succ_begin(), block.succ_end());
        for (Block *succ : succs) {
          removeUnreachable = true;
          for (auto &phi : succ->phis()) {
            phi.Remove(&block);
          }
        }
        block.clear();
        block.AddInst(new TrapInst({}));
      }
    } else {
      LLVM_DEBUG(llvm::dbgs() << "\t" << block.getName() << ":\n");
      // Replace individual instructions with constants.
      for (auto it = block.begin(); it != block.end(); ) {
        Inst *inst = &*it++;

        #ifndef NDEBUG
        LLVM_DEBUG(llvm::dbgs() << "\t\t" << *inst << "\n");
        for (unsigned i = 0, n = inst->GetNumRets(); i < n; ++i) {
          auto v = solver.GetValue(inst->GetSubValue(i));
          LLVM_DEBUG(llvm::dbgs() << "\t\t\t\t" << v << "\n");
        }
        #endif

        // Args are constant across an invocation, but not constant globally.
        if (inst->IsConstant() && !inst->Is(Inst::Kind::ARG)) {
          continue;
        }

        // Find individual sub-values that have uses.
        llvm::SmallVector<bool, 4> used(inst->GetNumRets());
        for (Use &use : inst->uses()) {
          used[(*use).Index()] = true;
        }

        // Find the value assigned to the instruction.
        llvm::SmallVector<Ref<Inst>, 4> newValues;
        unsigned numValues = 0;
        for (unsigned i = 0, n = inst->GetNumRets(); i < n; ++i) {
          auto ref = inst->GetSubValue(i);
          const auto &v = solver.GetValue(ref);

          // Find the relevant info from the original instruction.
          // The type is downgraded from V64 to I64 since constants are
          // not heap roots, thus they do not need to be tracked.
          Type type = ref.GetType() == Type::V64 ? Type::I64 : ref.GetType();
          auto annot = inst->GetAnnots();
          annot.Clear<CamlFrame>();

          // Create a mov instruction producing a constant value.
          Inst *newInst = nullptr;
          if (used[i]) {
            switch (v.GetKind()) {
              case Lattice::Kind::UNKNOWN:
              case Lattice::Kind::OVERDEFINED:
              case Lattice::Kind::POINTER:
              case Lattice::Kind::FLOAT_ZERO:
              case Lattice::Kind::MASK:
              case Lattice::Kind::RANGE: {
                break;
              }
              case Lattice::Kind::INT: {
                newInst = new MovInst(type, new ConstantInt(v.GetInt()), annot);
                break;
              }
              case Lattice::Kind::FLOAT: {
                newInst = new MovInst(type, new ConstantFloat(v.GetFloat()), annot);
                break;
              }
              case Lattice::Kind::FRAME: {
                newInst = new FrameInst(
                    type,
                    v.GetFrameObject(),
                    v.GetFrameOffset(),
                    annot
                );
                break;
              }
              case Lattice::Kind::GLOBAL: {
                Value *global = nullptr;
                Global *sym = v.GetGlobalSymbol();
                if (auto offset = v.GetGlobalOffset()) {
                  global = SymbolOffsetExpr::Create(sym, offset);
                } else {
                  global = sym;
                }
                newInst = new MovInst(type, global, annot);
                break;
              }
              case Lattice::Kind::UNDEFINED: {
                newInst = new UndefInst(type, annot);
                break;
              }
            }
          }

          // Add the new instruction prior to the replaced one. This ensures
          // constant return values are placed before the call instructions
          // producing them.
          if (newInst) {
            auto insert = inst->getIterator();
            while (insert->Is(Inst::Kind::PHI)) {
              ++insert;
            }
            block.insert(newInst, insert);
            newValues.push_back(newInst);
            numValues++;
            changed = true;
          } else {
            newValues.push_back(ref);
          }
        }

        // Replaces uses if any of them changed and erase the instruction if no
        // users are left, unless the instruction has side effects.
        if (numValues) {
          ++NumConstantsFolded;
          inst->replaceAllUsesWith(newValues);
          if (!inst->HasSideEffects()) {
            inst->eraseFromParent();
          }
        }
      }
    }
  }
  if (removeUnreachable) {
    func.RemoveUnreachable();
  }
  return changed;
}


// -----------------------------------------------------------------------------
const char *SCCPPass::kPassID = "sccp";

// -----------------------------------------------------------------------------
const char *SCCPPass::GetPassName() const
{
  return "Sparse Conditional Constant Propagation";
}

// -----------------------------------------------------------------------------
bool SCCPPass::Run(Prog &prog)
{
  SCCPSolver solver(prog, GetTarget());
  bool changed = false;
  for (auto &func : prog) {
    changed = Rewrite(func, solver) || changed;
  }
  return changed;
}
