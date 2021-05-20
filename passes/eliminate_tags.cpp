 // This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/ADT/Statistic.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/cfg.h"
#include "core/clone.h"
#include "core/func.h"
#include "core/insts.h"
#include "core/prog.h"
#include "core/target.h"
#include "passes/eliminate_tags.h"
#include "passes/tags/type_analysis.h"
#include "passes/tags/value_analysis.h"
#include "passes/tags/tagged_type.h"

using namespace tags;

#define DEBUG_TYPE "eliminate-tags"

STATISTIC(NumTypesRewritten, "Number of v64 replaced with i64");



// -----------------------------------------------------------------------------
const char *EliminateTagsPass::kPassID = DEBUG_TYPE;

// -----------------------------------------------------------------------------
class TypeRewriter final : public CloneVisitor {
public:
  TypeRewriter(TypeAnalysis &types) : types_(types) {}

  ~TypeRewriter() { Fixup(); }

  Type Map(Type ty, Inst *inst, unsigned idx) override
  {
    if (ty == Type::V64) {
      if (types_.Find(inst->GetSubValue(idx)).IsOdd()) {
        return Type::I64;
      } else {
        return ty;
      }
    } else {
      return ty;
    }
  }

private:
  TypeAnalysis &types_;
};

// -----------------------------------------------------------------------------
bool EliminateTagsPass::Run(Prog &prog)
{
  TypeAnalysis types(prog, GetTarget());
  //ValueAnalysis values(types, prog);
  //types.dump();
  bool changed = false;
 
  // Analyse values and identify candidates for simplification.

  // Rewrite V64 to I64 if the classification is odd.
  for (Func &func : prog) {
    for (Block &block : func) {
      for (auto it = block.begin(); it != block.end(); ) {
        Inst &inst = *it++;

        bool rewrite = false;
        for (unsigned i = 0, n = inst.GetNumRets(); i < n; ++i) {
          auto type = inst.GetType(i);
          auto val = types.Find(inst.GetSubValue(i));
          if (type == Type::V64 && val.IsOdd()) {
            rewrite = true;
          }
        }

        if (rewrite) {
          auto newInst = TypeRewriter(types).Clone(&inst);
          block.AddInst(newInst, &inst);
          inst.replaceAllUsesWith(newInst);
          inst.eraseFromParent();
          NumTypesRewritten++;
          changed = true;
        }
      }
    }
  }

  return changed;
}

// -----------------------------------------------------------------------------
const char *EliminateTagsPass::GetPassName() const
{
  return "Eliminate Tagged Integers";
}
