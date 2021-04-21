// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/PostOrderIterator.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/cfg.h"
#include "core/clone.h"
#include "core/func.h"
#include "core/prog.h"
#include "core/insts.h"
#include "passes/localize_select.h"

#define DEBUG_TYPE "localise-select"


// -----------------------------------------------------------------------------
const char *LocalizeSelectPass::kPassID = DEBUG_TYPE;

// -----------------------------------------------------------------------------
namespace {
class Cloner final : public CloneVisitor {
public:
  Cloner(Ref<Inst> fromi, Ref<Inst> toi)
  {
    insts_.emplace(fromi, toi);
  }

  Ref<Inst> Map(Ref<Inst> ref)
  {
    if (auto it = insts_.find(ref); it != insts_.end()) {
      return it->second;
    } else {
      return ref;
    }
  }

private:
  /// Instructions to replace.
  std::unordered_map<Ref<Inst>, Ref<Inst>> insts_;
};
}

// -----------------------------------------------------------------------------
bool LocalizeSelectPass::Run(Prog &prog)
{
  for (Func &func : prog) {
    for (Block &block : func) {
      for (auto it = block.begin(); it != block.end(); ) {
        auto *s = ::cast_or_null<SelectInst>(&*it++);
        if (!s) {
          continue;
        }
        auto cc = s->GetCond();
        if (cc->getParent() == &block || !cc->Is(Inst::Kind::CMP)) {
          continue;
        }
        auto *newCC = Cloner(nullptr, nullptr).Clone(cc.Get());
        block.AddInst(newCC, s);

        auto *newS = Cloner(cc, newCC).Clone(s);
        block.AddInst(newS, s);
        s->replaceAllUsesWith(newS);
        s->eraseFromParent();
      }
    }
  }
  return false;
}

// -----------------------------------------------------------------------------
const char *LocalizeSelectPass::GetPassName() const
{
  return "Select Condition Localisation";
}
