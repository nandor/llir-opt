// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/ADT/SmallPtrSet.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/cfg.h"
#include "core/clone.h"
#include "core/func.h"
#include "core/inst_visitor.h"
#include "core/insts.h"
#include "core/prog.h"
#include "passes/eliminate_select.h"



// -----------------------------------------------------------------------------
const char *EliminateSelectPass::kPassID = "eliminate-select";

// -----------------------------------------------------------------------------
const char *EliminateSelectPass::GetPassName() const
{
  return "Select Elimination";
}

// -----------------------------------------------------------------------------
static bool IsFunc(ConstRef<Inst> ref)
{
  if (auto movRef = ::cast_or_null<MovInst>(ref)) {
    if (auto g = ::cast_or_null<Global>(movRef->GetArg())) {
      return g->Is(Global::Kind::FUNC);
    }
  }
  return false;
}

// -----------------------------------------------------------------------------
namespace {
class Cloner final : public CloneVisitor {
public:
  Cloner(Ref<Inst> fromi, Ref<Inst> toi, Block *fromb, Block *tob)
  {
    insts_.emplace(fromi, toi);
    blocks_.emplace(fromb, tob);
  }

  Ref<Inst> Map(Ref<Inst> ref)
  {
    if (auto it = insts_.find(ref); it != insts_.end()) {
      return it->second;
    } else {
      return ref;
    }
  }

  Block *Map(Block *block)
  {
    if (auto it = blocks_.find(block); it != blocks_.end()) {
      return it->second;
    } else {
      return block;
    }
  }

private:
  /// Instructions to replace.
  std::unordered_map<Ref<Inst>, Ref<Inst>> insts_;
  /// Blocks to replace.
  std::unordered_map<Block *, Block *> blocks_;
};

class Visitor final : public InstVisitor<void> {
public:
  Visitor(Ref<SelectInst> select) : s_(select) { }

  void VisitInst(Inst &inst) { llvm_unreachable("not implemented"); }

  void VisitCallInst(CallInst &call)
  {
    auto &block = *call.getParent();
    auto &func = *block.getParent();
    auto *cont = call.GetCont();
    std::string blockName(block.GetName());

    // New continuation block.
    auto *newCont = new Block(blockName + "$cont");
    newCont->AddInst(new JumpInst(cont, {}));

    // True branch.
    auto *bLHS = new Block(blockName + "$lhs");
    auto *callLHS = Cloner(s_, s_->GetTrue(), cont, newCont).Clone(&call);
    bLHS->AddInst(callLHS);

    // False branch.
    auto *bRHS = new Block(blockName + "$rhs");
    auto *callRHS = Cloner(s_, s_->GetFalse(), cont, newCont).Clone(&call);
    bRHS->AddInst(callRHS);

    // Add the blocks.
    auto bt = block.getIterator();
    func.insertAfter(bt, newCont);
    func.insertAfter(bt, bLHS);
    func.insertAfter(bt, bRHS);

    // Replace the old call.
    block.AddInst(new JumpCondInst(s_->GetCond(), bLHS, bRHS, {}));

    // Deal with PHIs.
    for (auto ut = block.use_begin(); ut != block.use_end(); ) {
      Use &use = *ut++;
      if (auto *phi = ::cast_or_null<PhiInst>(use.getUser())) {
        use = newCont;
      }
    }

    // Merge point.
    llvm::SmallVector<Ref<Inst>, 4> phis;
    for (unsigned i = 0, n = call.type_size(); i < n; ++i) {
      auto *phi = new PhiInst(call.type(i), {});
      phi->Add(bLHS, callLHS->GetSubValue(i));
      phi->Add(bRHS, callRHS->GetSubValue(i));
      newCont->AddPhi(phi);
      phis.push_back(phi);
    }
    call.replaceAllUsesWith(phis);
    call.eraseFromParent();
  }

private:
  /// Underlying select.
  Ref<SelectInst> s_;
};
} // anonymous namespace

// -----------------------------------------------------------------------------
bool EliminateSelectPass::Run(Prog &prog)
{
  // Collect all selects.
  std::vector<Ref<SelectInst>> selects;
  for (auto &func : prog) {
    for (auto &block : func) {
      for (auto &inst : block) {
        auto select = ::cast_or_null<SelectInst>(&inst);
        if (!select || !IsFunc(select->GetTrue()) || !IsFunc(select->GetFalse())) {
          continue;
        }
        selects.push_back(select);
      }
    }
  }
  bool changed = false;
  for (auto &select : selects) {
    for (auto ut = select->user_begin(); ut != select->user_end(); ) {
      Visitor(select).Dispatch(*::cast<Inst>(*ut++));
      changed = true;
    }
    assert(select->use_empty() && "select uses remaining");
    select->eraseFromParent();
  }
  return changed;
}
