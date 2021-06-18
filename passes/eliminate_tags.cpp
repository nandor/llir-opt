 // This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/ADT/Statistic.h>
#include <llvm/Support/Debug.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/cfg.h"
#include "core/clone.h"
#include "core/func.h"
#include "core/insts.h"
#include "core/prog.h"
#include "core/target.h"
#include "passes/eliminate_tags.h"
#include "passes/tags/register_analysis.h"
#include "passes/tags/tagged_type.h"

using namespace tags;

#define DEBUG_TYPE "eliminate-tags"

STATISTIC(NumTypesRewritten, "Number of v64 replaced with i64");
STATISTIC(NumAddCmp, "Number of add-cmp pairs rewritten");
STATISTIC(NumConstFolded, "Number of instructions folded to zero/one");



// -----------------------------------------------------------------------------
const char *EliminateTagsPass::kPassID = DEBUG_TYPE;

// -----------------------------------------------------------------------------
class TypeRewriter final : public CloneVisitor {
public:
  TypeRewriter(llvm::SmallVectorImpl<TaggedType> &types) : types_(types) {}

  ~TypeRewriter() { Fixup(); }

  Type Map(Type ty, Inst *inst, unsigned idx) override
  {
    if (ty == Type::V64 && types_[idx].IsOddLike()) {
      return Type::I64;
    } else {
      return ty;
    }
  }

private:
  llvm::SmallVectorImpl<TaggedType> &types_;
};

// -----------------------------------------------------------------------------
class EliminateTags final {
public:
  EliminateTags(Prog &prog, const Target *target)
    : prog_(prog)
    , types_(prog, target)
  {
    LLVM_DEBUG(types_.dump(llvm::dbgs()));
  }

  /// Initial transformation, narrows the types of values.
  bool NarrowTypes();
  /// Runs peephole transformations until exhaustion.
  bool Peephole();
  /// Rewrite values which evaluate to constants.
  bool RewriteConst();

  /// Wrapper to try all peepholes.
  Inst *Peephole(Inst &inst);
  /// Peephole to simplify add-cmp.
  Inst *PeepholeAddCmp(Inst &inst);

private:
  Prog &prog_;
  RegisterAnalysis types_;
};

// -----------------------------------------------------------------------------
static bool PhiEqualTo(PhiInst *phi, Ref<Inst> op)
{
  std::unordered_set<PhiInst *> phis;
  std::queue<PhiInst *> q;

  phis.insert(phi);
  q.push(phi);

  while (!q.empty()) {
    auto *phi = q.front();
    q.pop();
    for (unsigned i = 0, n = phi->GetNumIncoming(); i < n; ++i) {
      auto value = phi->GetValue(i);
      if (value != op) {
        if (auto phiValue = ::cast_or_null<PhiInst>(value)) {
          auto *phiInst = phiValue.Get();
          if (phis.insert(phiInst).second) {
            q.push(phiInst);
          }
        } else {
          return false;
        }
      }
    }
  }
  return true;
}

// -----------------------------------------------------------------------------
bool EliminateTags::NarrowTypes()
{
  bool changed = false;
  for (Func &func : prog_) {
    // Gather argument instructions and rewrite type if all match.
    std::vector<FlaggedType> newTypes;
    {
      llvm::SmallVector<llvm::SmallVector<ArgInst *, 1>, 6> argsByIndex;
      for (Block &block : func) {
        for (Inst &inst : block) {
          if (auto *arg = ::cast_or_null<ArgInst>(&inst)) {
            auto idx = arg->GetIndex();
            argsByIndex.resize(idx + 1);
            argsByIndex[idx].push_back(arg);
          }
        }
      }
      for (auto &param : func.params()) {
        newTypes.push_back(param);
      }
      for (unsigned i = 0, n = argsByIndex.size(); i < n; ++i) {
        auto &args = argsByIndex[i];

        bool rewrite = !args.empty();
        for (auto &arg : args) {
          if (arg->GetType() != Type::V64) {
            rewrite = false;
            break;
          }
          rewrite = rewrite && types_.Find(arg->GetSubValue(0)).IsOddLike();
        }
        if (rewrite) {
          newTypes[i] = FlaggedType(Type::I64, newTypes[i].GetFlag());
        }
      }
      func.SetParameters(newTypes);
    }

    // Rewrite instructions and eliminate movs added by splits.
    for (auto *block : llvm::ReversePostOrderTraversal<Func *>(&func)) {
      for (auto it = block->begin(); it != block->end(); ) {
        Inst *inst = &*it++;

        bool rewrite = false;
        llvm::SmallVector<TaggedType, 4> types;
        for (unsigned i = 0, n = inst->GetNumRets(); i < n; ++i) {
          auto type = inst->GetType(i);
          auto val = types_.Find(inst->GetSubValue(i));
          if (type == Type::V64 && val.IsOddLike()) {
            rewrite = true;
          }
          types.push_back(val);
        }
        if (auto *mov = ::cast_or_null<MovInst>(inst)) {
          auto ref = mov->GetArg();
          Type ty = rewrite ? Type::I64 : mov->GetType();
          if (auto arg = ::cast_or_null<Inst>(ref); arg && ty == arg.GetType()) {
            types_.Erase(mov->GetSubValue(0));
            mov->replaceAllUsesWith(arg);
            mov->eraseFromParent();
          } else if (rewrite) {
            auto *newInst = new MovInst(Type::I64, ref, mov->GetAnnots());
            types_.Replace(
                inst->GetSubValue(0),
                newInst->GetSubValue(0),
                types[0]
            );
            block->AddInst(newInst, inst);
            inst->replaceAllUsesWith(newInst);
            inst->eraseFromParent();
          }
        } else if (auto *arg = ::cast_or_null<ArgInst>(inst)) {
          Type ty = newTypes[arg->GetIndex()].GetType();
          if (rewrite && ty != arg->GetType()) {
            auto *newInst = new ArgInst(ty, arg->GetIndex(), arg->GetAnnots());
            types_.Replace(
                arg->GetSubValue(0),
                newInst->GetSubValue(0),
                types[0]
            );
            block->AddInst(newInst, arg);
            arg->replaceAllUsesWith(newInst);
            arg->eraseFromParent();
          }
        } else if (rewrite) {
          auto newInst = TypeRewriter(types).Clone(inst);
          for (unsigned i = 0, n = types.size(); i < n; ++i) {
            types_.Replace(
                inst->GetSubValue(i),
                newInst->GetSubValue(i),
                types[i]
            );
          }
          block->AddInst(newInst, inst);
          inst->replaceAllUsesWith(newInst);
          inst->eraseFromParent();
        }
        if (rewrite) {
          NumTypesRewritten++;
          changed = true;
        }
      }
    }
    // Eliminate PHI cycles which were added and are now redundant.
    for (Block *block : llvm::ReversePostOrderTraversal<Func *>(&func)) {
      for (auto it = block->begin(); it != block->end(); ) {
        if (auto *phi = ::cast_or_null<PhiInst>(&*it++)) {
          for (unsigned i = 0, n = phi->GetNumIncoming(); i < n; ++i) {
            auto op = phi->GetValue(i);
            if (PhiEqualTo(phi, op)) {
              types_.Erase(phi->GetSubValue(0));
              phi->replaceAllUsesWith(op);
              phi->eraseFromParent();
              break;
            }
          }
        }
      }
    }
    // Eliminate empty blocks.
    for (auto it = func.begin(); it != func.end(); ) {
      Block *block = &*it++;
      // Find blocks which split edges and are empty.
      auto *jmp = ::cast_or_null<JumpInst>(block->GetTerminator());
      if (!jmp || block->size() != 1 || block->pred_size() != 1) {
        continue;
      }
      auto *pred = *block->pred_begin();
      auto *target = jmp->GetTarget();
      if (target == block) {
        continue;
      }
      // This step collapses pred -> block -> target into pred-> target.
      // If there is already such an edge, tranformation is forbidden.
      bool hasPredTargetEdge = false;
      for (Block *succ : pred->successors()) {
        if (succ == target) {
          hasPredTargetEdge = true;
          break;
        }
      }
      if (hasPredTargetEdge) {
        continue;
      }
      // Rewrite the terminator of the predecessor.
      {
        auto *term = pred->GetTerminator();
        for (auto it = term->op_begin(); it != term->op_end(); ) {
          Use &use = *it++;
          if ((*use).Get() == block) {
            use = target;
          }
        }
      }
      // Rewrite other uses (must be PHIs).
      block->replaceAllUsesWith(pred);
      block->eraseFromParent();
    }
  }
  return changed;
}

// -----------------------------------------------------------------------------
bool EliminateTags::Peephole()
{
  bool changed = false;
  for (Func &func : prog_) {
    for (auto *block : llvm::ReversePostOrderTraversal<Func*>(&func)) {
      for (auto it = block->begin(); it != block->end(); ) {
        if (auto *inst = Peephole(*it)) {
          it = inst->getIterator();
          changed = true;
        } else {
          ++it;
        }
      }
    }
  }
  return changed;
}

// -----------------------------------------------------------------------------
bool EliminateTags::RewriteConst()
{
  bool changed = false;
  for (Func &func : prog_) {
    for (auto &block : func) {
      for (auto it = block.begin(); it != block.end(); ) {
        Inst *inst = &*it++;

        // Args are constant across an invocation, but not constant globally.
        if (inst->IsConstant() && !inst->Is(Inst::Kind::ARG)) {
          continue;
        }

        // Replace individual sub-values.
        llvm::SmallVector<Ref<Inst>, 4> newValues;
        llvm::SmallVector<bool, 4> used(inst->GetNumRets(), false);
        for (Use &use : inst->uses()) {
          used[use.get().Index()] = true;
        }
        unsigned numValues = 0;
        for (unsigned i = 0, n = inst->GetNumRets(); i < n; ++i) {
          if (used[i]) {
            auto ref = inst->GetSubValue(i);
            auto ty = inst->GetType(i);
            auto type = types_.Find(ref);

            if (type.IsInt() && IsIntegerType(ty)) {
              if (auto v = type.GetInt().AsConst()) {
                auto *value = new ConstantInt(*v);
                auto *mov = new MovInst(ty, value, inst->GetAnnots());
                auto insert = inst->getIterator();
                while (insert->Is(Inst::Kind::PHI)) {
                  ++insert;
                }
                block.insert(mov, insert);
                newValues.push_back(mov);
                numValues++;
                continue;
              }
            }
          }
          newValues.push_back(inst->GetSubValue(i));
        }

        // If any sub-value changed, replace it.
        if (numValues) {
          ++NumConstFolded;
          inst->replaceAllUsesWith(newValues);
          for (unsigned i = 0, n = newValues.size(); i < n; ++i) {
            auto ref = inst->GetSubValue(i);
            if (newValues[i] != ref) {
              types_.Replace(ref, newValues[i], types_.Find(ref));
              changed = true;
            }
          }
          if (!inst->HasSideEffects()) {
            inst->eraseFromParent();
          }
        }
      }
    }
  }
  return changed;
}

// -----------------------------------------------------------------------------
Inst *EliminateTags::Peephole(Inst &inst)
{
  if (auto *newInst = PeepholeAddCmp(inst)) {
    return newInst;
  }
  return nullptr;
}

// -----------------------------------------------------------------------------
Inst *EliminateTags::PeepholeAddCmp(Inst &inst)
{
  auto *cmp = ::cast_or_null<CmpInst>(&inst);
  if (!cmp) {
    return nullptr;
  }
  auto add = ::cast_or_null<AddInst>(cmp->GetLHS());
  auto ref = ::cast_or_null<MovInst>(cmp->GetRHS());
  if (!add || !ref) {
    return nullptr;
  }
  auto off = ::cast_or_null<MovInst>(add->GetRHS());
  if (!off) {
    return nullptr;
  }
  auto ioff = ::cast_or_null<ConstantInt>(off->GetArg());
  auto iref = ::cast_or_null<ConstantInt>(ref->GetArg());
  if (!ioff || !iref || !ioff->GetValue().isOneValue()) {
    return nullptr;
  }
  if (!types_.Find(add->GetLHS()).IsEvenLike()) {
    return nullptr;
  }

  auto *block = inst.getParent();

  auto *newIRef = new ConstantInt(iref->GetValue() - ioff->GetValue());
  auto newRef = new MovInst(ref->GetType(), newIRef, ref->GetAnnots());
  block->AddInst(newRef, &inst);

  auto newCmp = new CmpInst(
      cmp->GetType(),
      add->GetLHS(),
      newRef,
      cmp->GetCC(),
      cmp->GetAnnots()
  );
  types_.Replace(cmp, newCmp);
  block->AddInst(newCmp, &inst);
  cmp->replaceAllUsesWith(newCmp);
  cmp->eraseFromParent();
  ++NumAddCmp;
  return newCmp;
}

// -----------------------------------------------------------------------------
bool EliminateTagsPass::Run(Prog &prog)
{
  EliminateTags pass(prog, GetTarget());
  bool changed = false;
  changed = pass.NarrowTypes() || changed;
  changed = pass.RewriteConst() || changed;
  changed = pass.Peephole() || changed;
  return changed;
}

// -----------------------------------------------------------------------------
const char *EliminateTagsPass::GetPassName() const
{
  return "Eliminate Tagged Integers";
}
