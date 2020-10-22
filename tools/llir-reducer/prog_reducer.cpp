// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <mutex>
#include <thread>
#include <sstream>
#include <llvm/ADT/SmallPtrSet.h>

#include "core/atom.h"
#include "core/block.h"
#include "core/cast.h"
#include "core/cfg.h"
#include "core/clone.h"
#include "core/data.h"
#include "core/func.h"
#include "core/insts.h"
#include "core/prog.h"
#include "prog_reducer.h"



// -----------------------------------------------------------------------------
class UnusedArgumentDeleter {
public:
  UnusedArgumentDeleter(Inst *inst)
    : args_(inst->value_op_begin(), inst->value_op_end())
  {
  }

  ~UnusedArgumentDeleter()
  {
    llvm::DenseSet<Value *> erased;
    for (Ref<Value> v : args_) {
      if (erased.count(v.Get())) {
        continue;
      }
      if (auto inst = ::cast_or_null<Inst>(v); inst && inst->use_empty()) {
        inst->eraseFromParent();
        erased.insert(&*v);
      }
      if (auto atom = ::cast_or_null<Atom>(v); atom && atom->use_empty()) {
        atom->eraseFromParent();
        erased.insert(&*v);
      }
    }
  }

private:
  llvm::SmallVector<Ref<Value>, 2> args_;
};

// -----------------------------------------------------------------------------
static Prog *Next(Prog *prog)
{
  return nullptr;
}

// -----------------------------------------------------------------------------
template<typename T>
static T *Next(T *elem)
{
  auto it = elem->getIterator();
  if (++it != elem->getParent()->end()) {
    return &*it;
  }
  if (auto *parent = Next(elem->getParent())) {
    return &*parent->begin();
  }
  return nullptr;
}

// -----------------------------------------------------------------------------
static bool HasAtoms(Prog &p)
{
  return !p.data_empty()
      && !p.data_begin()->empty()
      && !p.data_begin()->begin()->empty();
}

// -----------------------------------------------------------------------------
static bool HasInsts(Prog &p)
{
  return !p.empty()
      && !p.begin()->empty()
      && !p.begin()->begin()->empty();
}

// -----------------------------------------------------------------------------
static bool HasBlocks(Prog &p)
{
  return !p.empty() && !p.begin()->empty();
}

// -----------------------------------------------------------------------------
std::unique_ptr<Prog> ProgReducerBase::Reduce(
    std::unique_ptr<Prog> &&prog,
    const Timeout &timeout)
{
  bool changed = true;
  while (changed && !timeout) {
    changed = false;

    // Function simplification.
    if (!prog->empty()) {
      std::pair<std::unique_ptr<Prog>, Func *> current {
        std::move(prog),
        &*prog->begin()
      };

      while (current.second && !timeout) {
        if (auto prog = ReduceFunc(current.second)) {
          changed = true;
          current = std::move(*prog);
        } else {
          current = { std::move(current.first), Next(current.second) };
        }
      }

      prog = std::move(current.first);
    }

    // Try to simplify individual instructions.
    if (HasInsts(*prog)) {
      std::pair<std::unique_ptr<Prog>, Inst *> current {
        std::move(prog),
        &*prog->begin()->begin()->begin()
      };

      while (current.second && !timeout) {
        if (auto prog = ReduceInst(current.second)) {
          changed = true;
          current = std::move(*prog);
        } else {
          current = { std::move(current.first), Next(current.second) };
        }
      }

      prog = std::move(current.first);
    }

    // Jump threading + basic block simplification.
    if (HasBlocks(*prog)) {
      std::pair<std::unique_ptr<Prog>, Block *> current {
        std::move(prog),
        &*prog->begin()->begin()
      };

      while (current.second && !timeout) {
        if (auto prog = ReduceBlock(current.second)) {
          changed = true;
          current = std::move(*prog);
        } else {
          current = { std::move(current.first), Next(current.second) };
        }
      }

      prog = std::move(current.first);
    }
  }
  return std::move(prog);
}

// -----------------------------------------------------------------------------
ProgReducerBase::Bt ProgReducerBase::ReduceBlock(Block *b)
{
  Prog &p = *b->getParent()->getParent();
  if (auto *origJmp = ::cast_or_null<JumpInst>(b->GetTerminator())) {
    Block *origTarget = origJmp->GetTarget();
    if (origTarget->pred_size() != 1 || origTarget->HasAddressTaken()) {
      return std::nullopt;
    }

    auto &&[clonedProg, clonedJmp] = CloneT<JumpInst>(p, origJmp);
    auto *clonedBlock = clonedJmp->getParent();
    auto *clonedTarget = clonedJmp->GetTarget();

    clonedJmp->eraseFromParent();
    for (auto it = clonedTarget->begin(); it != clonedTarget->end(); ) {
      auto *inst = &*it++;
      if (auto *phi = ::cast_or_null<PhiInst>(inst)) {
        assert(phi->GetNumIncoming() == 1 && "invalid phi");
        assert(phi->GetBlock(0u) == clonedBlock && "invalid predecessor");
        phi->replaceAllUsesWith(phi->GetValue(0u));
        phi->eraseFromParent();
      } else {
        inst->removeFromParent();
        clonedBlock->AddInst(inst);
      }
    }
    clonedTarget->eraseFromParent();
    if (Verify(*clonedProg)) {
      return { { std::move(clonedProg), clonedBlock } };
    }
  }
  return std::nullopt;
}

// -----------------------------------------------------------------------------
static std::pair<std::unique_ptr<Prog>, Atom *> Clone(Prog &p, Atom *f)
{
  auto &&clonedProg = Clone(p);
  Atom *clonedAtom = nullptr;
  for (Data &data : clonedProg->data()) {
    for (Object &object : data) {
      for (Atom &atom : object) {
        if (atom.GetName() == f->GetName()) {
          clonedAtom = &atom;
        }
      }
    }
  }
  assert(clonedAtom && "function not cloned");
  return { std::move(clonedProg), clonedAtom };
}

// -----------------------------------------------------------------------------
static std::pair<std::unique_ptr<Prog>, Func *> Clone(Prog &p, Func *f)
{
  auto &&clonedProg = Clone(p);
  Func *clonedFunc = nullptr;
  for (Func &func : *clonedProg) {
    if (func.GetName() == f->GetName()) {
      clonedFunc = &func;
    }
  }
  assert(clonedFunc && "function not cloned");
  return { std::move(clonedProg), clonedFunc };
}

// -----------------------------------------------------------------------------
ProgReducerBase::Ft ProgReducerBase::ReduceFunc(Func *f)
{
  Prog &p = *f->getParent();

  // Try to empty the function.
  if (f->size() > 1 || f->begin()->size() > 1) {
    auto &&[clonedProg, clonedFunc] = Clone(p, f);
    clonedFunc->clear();
    auto *bb = new Block((".L" + clonedFunc->getName() + "_entry").str());
    bb->AddInst(new TrapInst({}));
    clonedFunc->AddBlock(bb);

    if (Verify(*clonedProg)) {
      return { { std::move(clonedProg), clonedFunc } };
    }
  }

  // Try to erase all references to the function.
  auto &&[clonedProg, clonedFunc] = Clone(p, f);
  for (auto it = clonedFunc->use_begin(); it != clonedFunc->use_end(); ) {
    Use *use = &*it++;
    if (auto *user = use->getUser()) {
      if (auto *mov = ::cast_or_null<MovInst>(user)) {
        *use = new ConstantInt(0);
        continue;
      } else {
        *use = nullptr;
      }
    } else {
      *use = nullptr;
    }
  }

  auto *nextFunc = Next(clonedFunc);
  clonedFunc->eraseFromParent();
  if (Verify(*clonedProg)) {
    return { { std::move(clonedProg), nextFunc } };
  }
  return std::nullopt;
}

// -----------------------------------------------------------------------------
template <typename T>
void ProgReducerBase::RemoveArg(CandidateList &cand, T *call)
{
  Prog &p = *call->getParent()->getParent()->getParent();
  for (unsigned i = 0, n = call->arg_size(); i < n; ++i) {
    auto &&[clonedProg, cloned] = CloneT<T>(p, call);
    UnusedArgumentDeleter deleted(cloned);

    std::vector<Inst *> args(cloned->arg_begin(), cloned->arg_end());
    args.erase(args.begin() + i);
    T *reduced = new T(
        cloned->GetType(),
        cloned->GetCallee(),
        args,
        std::min<unsigned>(cloned->GetNumFixedArgs(), args.size()),
        cloned->GetCallingConv(),
        cloned->GetAnnots()
    );

    cloned->getParent()->AddInst(reduced, cloned);
    cloned->replaceAllUsesWith(reduced);
    cloned->eraseFromParent();
    cand.emplace(std::move(clonedProg), static_cast<Inst *>(reduced));
  }
}

// -----------------------------------------------------------------------------
ProgReducerBase::It ProgReducerBase::VisitCall(CallInst *i)
{
  CandidateList cand;
  if (!i->type_empty()) {
    ReduceOperator(cand, i);
  } else {
    ReduceErase(cand, i);
  }

  ReduceOperator(cand, i);
  return Evaluate(std::move(cand));
}

// -----------------------------------------------------------------------------
ProgReducerBase::It ProgReducerBase::VisitInvoke(InvokeInst *i)
{
  llvm_unreachable("missing reducer");
  return std::nullopt;
}

// -----------------------------------------------------------------------------
ProgReducerBase::It ProgReducerBase::VisitRaise(RaiseInst *i)
{
  llvm_unreachable("missing reducer");
  return std::nullopt;
}

// -----------------------------------------------------------------------------
ProgReducerBase::It ProgReducerBase::VisitSyscall(SyscallInst *i)
{
  llvm_unreachable("missing reducer");
  return std::nullopt;
}

// -----------------------------------------------------------------------------
ProgReducerBase::It ProgReducerBase::VisitClone(CloneInst *i)
{
  llvm_unreachable("missing reducer");
  return std::nullopt;
}

// -----------------------------------------------------------------------------
ProgReducerBase::It ProgReducerBase::VisitTailCall(TailCallInst *call)
{
  llvm_unreachable("missing reducer");
  return std::nullopt;
}

// -----------------------------------------------------------------------------
ProgReducerBase::It ProgReducerBase::VisitStore(StoreInst *i)
{
  CandidateList cand;
  ReduceErase(cand, i);
  return Evaluate(std::move(cand));
}

// -----------------------------------------------------------------------------
ProgReducerBase::It ProgReducerBase::VisitVAStart(VAStartInst *i)
{
  CandidateList cand;
  ReduceErase(cand, i);
  return Evaluate(std::move(cand));
}

// -----------------------------------------------------------------------------
ProgReducerBase::It ProgReducerBase::VisitSet(SetInst *i)
{
  llvm_unreachable("missing reducer");
  return std::nullopt;
}

// -----------------------------------------------------------------------------
ProgReducerBase::It ProgReducerBase::VisitMov(MovInst *i)
{
  CandidateList cand;
  ReduceErase(cand, i);
  ReduceToUndef(cand, i);
  ReduceToOp(cand, i);
  ReduceToRet(cand, i);
  return Evaluate(std::move(cand));
}

// -----------------------------------------------------------------------------
ProgReducerBase::It ProgReducerBase::VisitArg(ArgInst *i)
{
  CandidateList cand;
  ReduceErase(cand, i);
  ReduceToUndef(cand, i);
  ReduceZero(cand, i);
  ReduceToOp(cand, i);
  ReduceToRet(cand, i);
  return Evaluate(std::move(cand));
}

// -----------------------------------------------------------------------------
ProgReducerBase::It ProgReducerBase::VisitSwitch(SwitchInst *inst)
{
  Prog &p = *inst->getParent()->getParent()->getParent();
  CandidateList cand;

  // Replace with a jump.
  for (unsigned i = 0, n = inst->getNumSuccessors(); i < n; ++i) {
    auto &&[clonedProg, clonedInst] = CloneT<SwitchInst>(p, inst);

    Block *from = clonedInst->getParent();
    Block *to = clonedInst->getSuccessor(i);

    for (unsigned j = 0; j < n; ++j) {
      if (i != j) {
        RemoveEdge(from, clonedInst->getSuccessor(j));
      }
    }

    JumpInst *jumpInst;
    {
      UnusedArgumentDeleter deleter(clonedInst);
      jumpInst = new JumpInst(to, clonedInst->GetAnnots());
      from->AddInst(jumpInst, clonedInst);
      clonedInst->eraseFromParent();
    }

    from->getParent()->RemoveUnreachable();
    cand.emplace(std::move(clonedProg), jumpInst);
  }

  // Remove all branches but one.
  for (unsigned i = 0, n = inst->getNumSuccessors(); i < n; ++i) {
    auto &&[clonedProg, clonedInst] = CloneT<SwitchInst>(p, inst);

    Block *from = clonedInst->getParent();

    SwitchInst *switchInst;
    {
      UnusedArgumentDeleter deleter(clonedInst);

      std::vector<Block *> succs;
      for (unsigned j = 0; j < n; ++j) {
        Block *to = clonedInst->getSuccessor(j);
        if (j == i) {
          RemoveEdge(from, to);
        } else {
          succs.push_back(to);
        }
      }

      switchInst = new SwitchInst(
          clonedInst->GetIdx(),
          succs,
          clonedInst->GetAnnots()
      );

      from->AddInst(switchInst, clonedInst);
      clonedInst->eraseFromParent();
    }

    from->getParent()->RemoveUnreachable();
    cand.emplace(std::move(clonedProg), switchInst);
  }

  return Evaluate(std::move(cand));
}

// -----------------------------------------------------------------------------
ProgReducerBase::It ProgReducerBase::VisitJmp(JumpInst *i)
{
  Prog &p = *i->getParent()->getParent()->getParent();
  auto &&[clonedProg, clonedInst] = CloneT<JumpInst>(p, i);

  Block *from = clonedInst->getParent();
  Block *to = clonedInst->GetTarget();

  TrapInst *trapInst;
  {
    UnusedArgumentDeleter deleter(i);
    trapInst = new TrapInst({});
    from->AddInst(trapInst);
    clonedInst->eraseFromParent();
    RemoveEdge(from, to);
  }

  from->getParent()->RemoveUnreachable();

  if (Verify(*clonedProg)) {
    return { { std::move(clonedProg), trapInst } };
  }
  return std::nullopt;
}

// -----------------------------------------------------------------------------
ProgReducerBase::It ProgReducerBase::VisitJcc(JumpCondInst *i)
{
  auto ToJump = [this, i](bool flag) -> Candidate {
    Prog &p = *i->getParent()->getParent()->getParent();
    auto &&[clonedProg, cloned] = CloneT<JumpCondInst>(p, i);

    Block *from = cloned->getParent();
    Block *to = flag ? cloned->GetTrueTarget() : cloned->GetFalseTarget();
    Block *other = flag ? cloned->GetFalseTarget() : cloned->GetTrueTarget();

    JumpInst *jumpInst;
    {
      UnusedArgumentDeleter deleter(cloned);
      jumpInst = new JumpInst(to, cloned->GetAnnots());
      from->AddInst(jumpInst);
      cloned->eraseFromParent();
      RemoveEdge(from, other);
    }

    from->getParent()->RemoveUnreachable();

    return { std::move(clonedProg), jumpInst };
  };

  CandidateList cand;
  cand.emplace(std::move(ToJump(true)));
  cand.emplace(std::move(ToJump(false)));
  return Evaluate(std::move(cand));
}

// -----------------------------------------------------------------------------
ProgReducerBase::It ProgReducerBase::VisitReturn(ReturnInst *i)
{
  Prog &p = *i->getParent()->getParent()->getParent();
  auto &&[clonedProg, cloned] = Clone(p, i);
  UnusedArgumentDeleter deleter(cloned);

  auto *trap = new TrapInst({});
  cloned->getParent()->AddInst(trap, cloned);
  cloned->eraseFromParent();
  if (Verify(*clonedProg)) {
    return { { std::move(clonedProg), trap } };
  }
  return std::nullopt;
}

// -----------------------------------------------------------------------------
ProgReducerBase::It ProgReducerBase::VisitPhi(PhiInst *phi)
{
  Prog &p = *phi->getParent()->getParent()->getParent();

  // Prepare annotations for the new instructions.
  AnnotSet annot = phi->GetAnnots();
  annot.Clear<CamlFrame>();

  // Find the PHI type.
  Type ty = phi->GetType();

  // Helper to find an insert point after phis.
  auto GetInsertPoint = [](Inst *inst) {
    auto it = inst->getIterator();
    while (it != inst->getParent()->end() && it->Is(Inst::Kind::PHI)) {
      ++it;
    }
    return &*it;
  };

  CandidateList cand;
  {
    auto &&[clonedProg, clonedInst] = Clone(p, phi);
    UnusedArgumentDeleter deleter(clonedInst);

    auto *undef = new UndefInst(ty, annot);
    clonedInst->getParent()->AddInst(undef, GetInsertPoint(clonedInst));
    clonedInst->replaceAllUsesWith(undef);
    auto *next = &*std::next(clonedInst->getIterator());
    clonedInst->eraseFromParent();
    cand.emplace(std::move(clonedProg), next);
  }

  {
    auto &&[clonedProg, clonedInst] = Clone(p, phi);
    UnusedArgumentDeleter deleter(clonedInst);

    auto *undef = new MovInst(ty, GetZero(ty), annot);
    clonedInst->getParent()->AddInst(undef, GetInsertPoint(clonedInst));
    clonedInst->replaceAllUsesWith(undef);
    auto *next = &*std::next(clonedInst->getIterator());
    clonedInst->eraseFromParent();
    cand.emplace(std::move(clonedProg), next);
  }

  return Evaluate(std::move(cand));
}

// -----------------------------------------------------------------------------
ProgReducerBase::It ProgReducerBase::VisitX86_FPUControlInst(
    X86_FPUControlInst *i)
{
  CandidateList cand;
  ReduceErase(cand, i);
  return Evaluate(std::move(cand));
}

// -----------------------------------------------------------------------------
ProgReducerBase::It ProgReducerBase::Visit(Inst *i)
{
  llvm_unreachable("missing reducer");
  return std::nullopt;
}

// -----------------------------------------------------------------------------
ProgReducerBase::It ProgReducerBase::VisitUndef(UndefInst *i)
{
  CandidateList cand;
  ReduceErase(cand, i);
  return Evaluate(std::move(cand));
}

// -----------------------------------------------------------------------------
ProgReducerBase::It ProgReducerBase::ReduceOperator(Inst *i)
{
  CandidateList cand;
  ReduceOperator(cand, i);
  return Evaluate(std::move(cand));
}

// -----------------------------------------------------------------------------
void ProgReducerBase::ReduceOperator(CandidateList &cand, Inst *i)
{
  ReduceErase(cand, i);
  ReduceToUndef(cand, i);
  ReduceZero(cand, i);
  ReduceToArg(cand, i);
  ReduceToOp(cand, i);
  ReduceToRet(cand, i);
}

// -----------------------------------------------------------------------------
void ProgReducerBase::ReduceToOp(CandidateList &cand, Inst *inst)
{
  if (inst->GetNumRets() == 0) {
    return;
  }
  Prog &p = *inst->getParent()->getParent()->getParent();
  for (unsigned i = 0, n = inst->size(); i < n; ++i) {
    Ref<Value> value = *(inst->value_op_begin() + i);
    if (Ref<Inst> op = ::cast_or_null<Inst>(value)) {
      if (inst->GetType(0) != op.GetType()) {
        continue;
      }

      // Clone & replace with arg.
      auto &&[clonedProg, clonedInst] = Clone(p, inst);
      UnusedArgumentDeleter deleter(clonedInst);

      auto clonedOp = ::cast<Inst>(*(clonedInst->value_op_begin() + i));
      auto *next = &*std::next(clonedInst->getIterator());
      clonedInst->replaceAllUsesWith(clonedOp);
      clonedInst->eraseFromParent();

      cand.emplace(std::move(clonedProg), next);
    }
  }
}

// -----------------------------------------------------------------------------
void ProgReducerBase::ReduceToRet(CandidateList &cand, Inst *inst)
{
  Prog &p = *inst->getParent()->getParent()->getParent();
  for (unsigned i = 0, n = inst->size(); i < n; ++i) {
    Ref<Value> value = *(inst->value_op_begin() + i);
    if (Ref<Inst> op = ::cast_or_null<Inst>(value)) {
      // Clone & replace with arg.
      auto &&[clonedProg, clonedInst] = Clone(p, inst);
      UnusedArgumentDeleter deleter(clonedInst);

      auto clonedOp = cast<Inst>(*(clonedInst->value_op_begin() + i));
      Inst *returnInst = new ReturnInst(clonedOp, {});

      Block *clonedParent = clonedInst->getParent();
      clonedParent->AddInst(returnInst, clonedInst);
      for (auto it = clonedInst->getIterator(); it != clonedParent->end(); ) {
        (&*it++)->eraseFromParent();
      }
      clonedParent->getParent()->RemoveUnreachable();

      cand.emplace(std::move(clonedProg), returnInst);
    }
  }
}

// -----------------------------------------------------------------------------
void ProgReducerBase::ReduceToUndef(CandidateList &cand, Inst *inst)
{
  if (inst->GetNumRets() == 0) {
    return;
  }

  Prog &p = *inst->getParent()->getParent()->getParent();
  auto &&[clonedProg, clonedInst] = Clone(p, inst);
  UnusedArgumentDeleter deleter(clonedInst);

  AnnotSet annot = clonedInst->GetAnnots();
  annot.Clear<CamlFrame>();

  Inst *undef = new UndefInst(clonedInst->GetType(0), annot);
  clonedInst->getParent()->AddInst(undef, clonedInst);
  clonedInst->replaceAllUsesWith(undef);
  clonedInst->eraseFromParent();
  cand.emplace(std::move(clonedProg), undef);
}

// -----------------------------------------------------------------------------
void ProgReducerBase::ReduceZero(CandidateList &cand, Inst *inst)
{
  if (inst->GetNumRets() == 0) {
    return;
  }

  Prog &p = *inst->getParent()->getParent()->getParent();
  auto &&[clonedProg, clonedInst] = Clone(p, inst);
  UnusedArgumentDeleter deleter(clonedInst);

  AnnotSet annot = clonedInst->GetAnnots();
  annot.Clear<CamlFrame>();

  Type type = clonedInst->GetType(0);

  Inst *mov = new MovInst(type, GetZero(type), annot);
  clonedInst->getParent()->AddInst(mov, clonedInst);
  clonedInst->replaceAllUsesWith(mov);
  clonedInst->eraseFromParent();
  cand.emplace(std::move(clonedProg), mov);
}

// -----------------------------------------------------------------------------
void ProgReducerBase::ReduceErase(CandidateList &cand, Inst *inst)
{
  if (!inst->use_empty()) {
    return;
  }

  Prog &p = *inst->getParent()->getParent()->getParent();
  auto &&[clonedProg, clonedInst] = Clone(p, inst);
  UnusedArgumentDeleter deleter(clonedInst);

  Inst *next = &*std::next(clonedInst->getIterator());
  clonedInst->eraseFromParent();
  cand.emplace(std::move(clonedProg), next);
}

// -----------------------------------------------------------------------------
void ProgReducerBase::ReduceToArg(CandidateList &cand, Inst *inst)
{
  if (inst->GetNumRets() == 0) {
    return;
  }

  auto params = inst->getParent()->getParent()->params();
  Type ty = inst->GetType(0);
  for (unsigned i = 0, n = params.size(); i < n; ++i) {
    if (params[i] == ty) {
      Prog &p = *inst->getParent()->getParent()->getParent();
      auto &&[clonedProg, clonedInst] = Clone(p, inst);
      UnusedArgumentDeleter deleter(clonedInst);

      Inst *arg = new ArgInst(ty, new ConstantInt(i), clonedInst->GetAnnots());
      clonedInst->getParent()->AddInst(arg, clonedInst);
      clonedInst->replaceAllUsesWith(arg);
      clonedInst->eraseFromParent();
      cand.emplace(std::move(clonedProg), arg);
    }
  }
}

// -----------------------------------------------------------------------------
void ProgReducerBase::ReduceToTrap(CandidateList &cand, Inst *inst)
{
  Prog &p = *inst->getParent()->getParent()->getParent();

  auto &&[clonedProg, clonedInst] = Clone(p, inst);
  UnusedArgumentDeleter deleter(clonedInst);

  Inst *trap = new TrapInst(clonedInst->GetAnnots());
  clonedInst->getParent()->AddInst(trap, clonedInst);
  clonedInst->replaceAllUsesWith(trap);
  clonedInst->eraseFromParent();
  cand.emplace(std::move(clonedProg), trap);
}

// -----------------------------------------------------------------------------
ProgReducerBase::It ProgReducerBase::Evaluate(CandidateList &&candidates)
{
  It best = std::nullopt;

  std::mutex lock;
  std::vector<std::thread> threads;
  for (unsigned i = 0; i < threads_; ++i) {
    threads.emplace_back([this, &threads, &lock, &candidates, &best] {
      for (;;) {
        Candidate cand;
        {
          std::lock_guard<std::mutex> guard(lock);
          if (candidates.empty() || best) {
            return;
          }
          cand = std::move(candidates.front());
          candidates.pop();
        }

        if (Verify(*cand.first)) {
          std::lock_guard<std::mutex> guard(lock);
          if (!best) {
            best = std::move(cand);
          }
        }
      }
    });
  }

  for (std::thread &thread : threads) {
    thread.join();
  }
  return best;
}

// -----------------------------------------------------------------------------
void ProgReducerBase::RemoveEdge(Block *from, Block *to)
{
  for (PhiInst &phi : to->phis()) {
    phi.Remove(from);
  }
}

// -----------------------------------------------------------------------------
Constant *ProgReducerBase::GetZero(Type type)
{
  switch (type) {
    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::I64:
    case Type::V64:
    case Type::I128: {
      return new ConstantInt(0);
    }
    case Type::F32: case Type::F64: case Type::F80: {
      return new ConstantFloat(0.0f);
    }
  }
  llvm_unreachable("invalid type");
}
