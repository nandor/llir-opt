// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/PostOrderIterator.h>

#include "core/cast.h"
#include "core/cfg.h"
#include "core/func.h"
#include "core/prog.h"
#include "emitter/coq/coqemitter.h"



// -----------------------------------------------------------------------------
CoqEmitter::CoqEmitter(llvm::raw_ostream &os)
  : os_(os)
{
}

// -----------------------------------------------------------------------------
void CoqEmitter::Write(const Prog &prog)
{
  os_ << "Require Import Coq.ZArith.ZArith.\n";
  os_ << "Require Import LLIR.LLIR.\n";
  os_ << "Require Import LLIR.Maps.\n";
  os_ << "Require Import Coq.Lists.List.\n";
  os_ << "Import ListNotations.\n";

  os_ << "\n";
  for (const Func &func : prog) {
    Write(func);
  }
}

// -----------------------------------------------------------------------------
void CoqEmitter::Write(const Func &func)
{
  os_ << "Definition " << func.getName() << ": func := \n";

  // Stack object descriptors.
  os_.indent(2) << "{| fn_stack :=\n";
  os_.indent(4) << "<< ";
  {
    auto objs = func.objects();
    for (unsigned i = 0, n = objs.size(); i < n; ++i) {
      if (i != 0) {
        os_ << ";  ";
      }

      auto &obj = objs[i];
      os_ << "(";
      os_ << (obj.Index + 1) << "%positive";
      os_ << ", ";
      os_ << "{| obj_size := " << obj.Size << "%positive";
      os_ << "; obj_align := " << obj.Alignment << "%positive";
      os_ << "|}";
      os_ << ")";
      os_ << "\n";
      os_.indent(4);
    }
  }
  os_ << ">>\n";

  // Build a map of instruction and block indices.
  llvm::ReversePostOrderTraversal<const Func*> blockOrder(&func);
  std::unordered_map<const Inst *, unsigned> insts;
  std::unordered_map<const Block *, unsigned> blocks;
  for (const Block *block : blockOrder) {
    std::optional<unsigned> firstNonPhi;
    for (const Inst &inst : *block) {
      unsigned idx = insts.size() + 1;
      insts[&inst] = idx;
      if (!inst.Is(Inst::Kind::PHI) && !firstNonPhi) {
        firstNonPhi = idx;
      }
    }
    assert(firstNonPhi && "empty block");
    blocks[block] = *firstNonPhi;
  }

  // Instructions.
  std::optional<unsigned> entry;
  os_.indent(2) << "; fn_insts :=\n";
  os_.indent(4) << "<< ";
  {
    for (const Block *block : blockOrder) {
      for (auto it = block->begin(), end = block->end(); it != end; ++it) {
        if (it->Is(Inst::Kind::PHI)) {
          continue;
        }

        unsigned idx = insts[&*it];
        if (entry) {
          os_ << ";  ";
        } else {
          entry = idx;
        }

        os_ << "(" << idx << "%positive, ";

        os_ << "LLJmp 1%positive";

        os_ << ")\n";
        os_.indent(4);
      }
    }
  }
  os_ << ">>\n";

  // PHIs for each block.
  os_.indent(2) << "; fn_phis := \n";
  os_.indent(4) << "<<";
  {
    bool first = true;
    for (const Block *block : blockOrder) {
      llvm::SmallVector<const PhiInst *, 10> phis;
      for (const Inst &inst : *block) {
        if (auto *phi = ::dyn_cast_or_null<const PhiInst>(&inst)) {
          phis.push_back(phi);
        } else {
          break;
        }
      }
      if (!phis.empty()) {
        if (!first) {
          os_ << "; ";
        }
        first = false;

        os_ << " (" << blocks[block] << "%positive\n";
        os_.indent(7) << ", [ ";
        for (unsigned i = 0, n = phis.size(); i < n; ++i) {
          if (i != 0) {
            os_ << "; ";
          }

          const PhiInst *phi = phis[i];
          os_ << "LLPhi\n";
          os_.indent(11) << "[ ";
          for (unsigned j = 0, m = phi->GetNumIncoming(); j < m; ++j) {
            if (j != 0) {
              os_ << "; ";
            }

            const Block *block = phi->GetBlock(j);
            const Value *val = phi->GetValue(j);

            os_ << "(";
            os_ << insts[block->GetTerminator()] << "%positive";
            os_ << ", ";
            os_ << "xH";
            os_ << ")\n";
            os_.indent(11);
          }
          os_ << "]\n";
          os_.indent(11) << insts[phi] << "%positive\n";
          os_.indent(9);
        }
        os_ << "]\n";
        os_.indent(7) << ")\n";
        os_.indent(4);
      }
    }
  }
  os_ << ">>\n";

  // Entry point.
  assert(entry && "missing entry point");
  os_.indent(2) << "; fn_entry := " << *entry << "%positive\n";
  os_.indent(2) << "|}.\n\n";
}
