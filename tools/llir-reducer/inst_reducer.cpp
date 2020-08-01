// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <sstream>
#include <llvm/ADT/SmallPtrSet.h>

#include "core/atom.h"
#include "core/block.h"
#include "core/cast.h"
#include "core/clone.h"
#include "core/cfg.h"
#include "core/data.h"
#include "core/func.h"
#include "core/prog.h"
#include "core/insts.h"
#include "inst_reducer.h"



// -----------------------------------------------------------------------------
class UnusedArgumentDeleter {
public:
  UnusedArgumentDeleter(Inst *inst)
    : args_(inst->value_op_begin(), inst->value_op_end())
  {
  }

  ~UnusedArgumentDeleter()
  {
    for (Value *v : args_) {
      if (auto *inst = ::dyn_cast_or_null<Inst>(v); inst && inst->use_empty()) {
        inst->eraseFromParent();
      }
      if (auto *atom = ::dyn_cast_or_null<Atom>(v); atom && atom->use_empty()) {
        atom->eraseFromParent();
      }
    }
  }

private:
  llvm::SmallVector<Value *, 2> args_;
};


// -----------------------------------------------------------------------------
static Inst *Next(Inst *inst)
{
  Block *block = inst->getParent();
  Func *func = block->getParent();
  Prog *prog = func->getParent();

  auto instIt = inst->getIterator();
  if (++instIt != block->end()) {
    return &*instIt;
  }

  auto blockIt = block->getIterator();
  if (++blockIt != func->end()) {
    return &*blockIt->begin();
  }

  auto funcIt = func->getIterator();
  if (++funcIt != prog->end()) {
    return &*funcIt->begin()->begin();
  }

  return nullptr;
}

// -----------------------------------------------------------------------------
static Block *Next(Block *block)
{
  Func *func = block->getParent();
  Prog *prog = func->getParent();

  auto blockIt = block->getIterator();
  if (++blockIt != func->end()) {
    return &*blockIt;
  }

  auto funcIt = func->getIterator();
  if (++funcIt != prog->end()) {
    return &*funcIt->begin();
  }

  return nullptr;
}

// -----------------------------------------------------------------------------
std::unique_ptr<Prog> InstReducerBase::Reduce(std::unique_ptr<Prog> &&prog)
{
  std::unique_ptr<Prog> result(std::move(prog));

  bool changed;
  do {
    changed = false;
    // Try to simplify individual instructions.
    {
      std::pair<std::unique_ptr<Prog>, Inst *> current {
        std::move(result),
        &*result->begin()->begin()->begin()
      };

      while (current.second) {
        if (auto result = ReduceInst(*current.first, current.second)) {
          changed = true;
          current = std::move(*result);
        } else {
          current = { std::move(current.first), Next(current.second) };
        }
      }

      result = std::move(current.first);
    }

    // Jump threading + basic block simplification.
    {
      std::pair<std::unique_ptr<Prog>, Block *> current {
        std::move(result),
        &*result->begin()->begin()
      };

      while (current.second) {
        if (auto result = ReduceBlock(*current.first, current.second)) {
          changed = true;
          current = std::move(*result);
        } else {
          current = { std::move(current.first), Next(current.second) };
        }
      }

      result = std::move(current.first);
    }
  } while (changed);
  return std::move(result);
}

// -----------------------------------------------------------------------------
InstReducerBase::It InstReducerBase::ReduceInst(Prog &p, Inst *i)
{
  // Mutate the instruction based on its kind.
  switch (i->GetKind()) {
    case Inst::Kind::CALL:      return ReduceCall(p, static_cast<CallInst *>(i));
    case Inst::Kind::TCALL:     return ReduceTailCall(p, static_cast<TailCallInst *>(i));
    case Inst::Kind::INVOKE:    return ReduceInvoke(p, static_cast<InvokeInst *>(i));
    case Inst::Kind::TINVOKE:   llvm_unreachable("TINVOKE");
    case Inst::Kind::SYSCALL:   llvm_unreachable("SYSCALL");
    case Inst::Kind::RET:       return ReduceRet(p, static_cast<ReturnInst *>(i));
    case Inst::Kind::JCC:       return ReduceJcc(p, static_cast<JumpCondInst *>(i));
    case Inst::Kind::JI:        llvm_unreachable("JI");
    case Inst::Kind::JMP:       return ReduceJmp(p, static_cast<JumpInst *>(i));
    case Inst::Kind::SWITCH:    return ReduceSwitch(p, static_cast<SwitchInst *>(i));
    case Inst::Kind::TRAP:      return std::nullopt;
    case Inst::Kind::LD:        return ReduceLoad(p, static_cast<LoadInst *>(i));
    case Inst::Kind::ST:        return ReduceStore(p, static_cast<StoreInst *>(i));
    case Inst::Kind::CMPXCHG:   llvm_unreachable("CMPXCHG");
    case Inst::Kind::XCHG:      llvm_unreachable("XCHG");
    case Inst::Kind::SET:       llvm_unreachable("SET");
    case Inst::Kind::VASTART:   llvm_unreachable("VASTART");
    case Inst::Kind::ALLOCA:    llvm_unreachable("ALLOCA");
    case Inst::Kind::ARG:       return ReduceArg(p, static_cast<ArgInst *>(i));
    case Inst::Kind::FRAME:     return ReduceFrame(p, static_cast<FrameInst *>(i));
    case Inst::Kind::UNDEF:     return ReduceUndef(p, static_cast<UndefInst *>(i));
    case Inst::Kind::RDTSC:     return ReduceRdtsc(p, static_cast<RdtscInst *>(i));
    case Inst::Kind::FNSTCW:    return ReduceFNStCw(p, static_cast<FNStCwInst *>(i));
    case Inst::Kind::FLDCW:     return ReduceFLdCw(p, static_cast<FLdCwInst *>(i));
    case Inst::Kind::MOV:       return ReduceMov(p, static_cast<MovInst *>(i));
    case Inst::Kind::SELECT:    return ReduceSelect(p, static_cast<SelectInst *>(i));
    case Inst::Kind::PHI:       return ReducePhi(p, static_cast<PhiInst *>(i));

    case Inst::Kind::ABS:
    case Inst::Kind::NEG:
    case Inst::Kind::SQRT:
    case Inst::Kind::SIN:
    case Inst::Kind::COS:
    case Inst::Kind::SEXT:
    case Inst::Kind::ZEXT:
    case Inst::Kind::XEXT:
    case Inst::Kind::FEXT:
    case Inst::Kind::TRUNC:
    case Inst::Kind::EXP:
    case Inst::Kind::EXP2:
    case Inst::Kind::LOG:
    case Inst::Kind::LOG2:
    case Inst::Kind::LOG10:
    case Inst::Kind::FCEIL:
    case Inst::Kind::FFLOOR:
    case Inst::Kind::POPCNT:
    case Inst::Kind::CLZ:
    case Inst::Kind::CTZ:
      return ReduceUnary(p, static_cast<UnaryInst *>(i));

    case Inst::Kind::ADD:
    case Inst::Kind::AND:
    case Inst::Kind::CMP:
    case Inst::Kind::UDIV:
    case Inst::Kind::SDIV:
    case Inst::Kind::UREM:
    case Inst::Kind::SREM:
    case Inst::Kind::MUL:
    case Inst::Kind::OR:
    case Inst::Kind::ROTL:
    case Inst::Kind::ROTR:
    case Inst::Kind::SLL:
    case Inst::Kind::SRA:
    case Inst::Kind::SRL:
    case Inst::Kind::SUB:
    case Inst::Kind::XOR:
    case Inst::Kind::POW:
    case Inst::Kind::COPYSIGN:
    case Inst::Kind::UADDO:
    case Inst::Kind::UMULO:
    case Inst::Kind::USUBO:
    case Inst::Kind::SADDO:
    case Inst::Kind::SMULO:
    case Inst::Kind::SSUBO:
      return ReduceBinary(p, static_cast<BinaryInst *>(i));
  }
}

// -----------------------------------------------------------------------------
InstReducerBase::Bt InstReducerBase::ReduceBlock(Prog &p, Block *b)
{
  if (auto *origJmp = ::dyn_cast_or_null<JumpInst>(b->GetTerminator())) {
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
      if (auto *phi = ::dyn_cast_or_null<PhiInst>(inst)) {
        assert(phi->GetNumIncoming() == 1 && "invalid phi");
        assert(phi->GetBlock(0u) == clonedBlock && "invalid predecessor");
        auto *value = phi->GetValue(0u);
        if (auto *inst = ::dyn_cast_or_null<Inst>(value)) {
          inst->SetAnnot(inst->GetAnnot().Union(phi->GetAnnot()));
        }
        phi->replaceAllUsesWith(value);
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
template<typename... Args>
InstReducerBase::It InstReducerBase::TryReducer(
    Inst * (InstReducerBase::*f)(Inst *, Args...),
    Prog &p,
    Inst *i,
    Args... args)
{
  auto &&[clonedProg, clonedInst] = Clone(p, i);
  if (auto *reduced = (this->*f)(clonedInst, args...)) {
    // Continue if verification passes.
    if (Verify(*clonedProg)) {
      return { { std::move(clonedProg), static_cast<Inst *>(reduced) } };
    }
  }
  return std::nullopt;
}

// -----------------------------------------------------------------------------
template <typename T>
InstReducerBase::It InstReducerBase::RemoveArg(Prog &p, T *call)
{
  for (unsigned i = 0, n = call->GetNumArgs(); i < n; ++i) {
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
        cloned->GetAnnot()
    );

    cloned->getParent()->AddInst(reduced, cloned);
    cloned->replaceAllUsesWith(reduced);
    cloned->eraseFromParent();
    if (Verify(*clonedProg)) {
      return { { std::move(clonedProg), static_cast<Inst *>(reduced) } };
    }
  }

  return std::nullopt;
}

// -----------------------------------------------------------------------------
InstReducerBase::It InstReducerBase::ReduceCall(Prog &p, CallInst *i)
{
  if (auto ty = i->GetType()) {
    if (It r = ReduceOperator(p, i)) {
      return r;
    }
  } else {
    if (It r = TryReducer(&InstReducerBase::ReduceErase, p, i)) {
      return r;
    }
  }

  if (It r = RemoveArg<CallInst>(p, i)) {
    return r;
  }

  return std::nullopt;
}

// -----------------------------------------------------------------------------
InstReducerBase::It InstReducerBase::ReduceInvoke(Prog &p, InvokeInst *i)
{
  llvm_unreachable("missing reducer");
}

// -----------------------------------------------------------------------------
InstReducerBase::It InstReducerBase::ReduceTailCall(Prog &p, TailCallInst *call)
{
  if (It r = TryReducer(&InstReducerBase::ReduceTrap, p, call)) {
    return r;
  }
  if (It r = RemoveArg<TailCallInst>(p, call)) {
    return r;
  }

  if (auto ty = call->GetType()) {
    for (unsigned i = 0, n = call->size(); i < n; ++i) {
      auto *arg = *(call->value_op_begin() + i);
      if (auto *inst = ::dyn_cast_or_null<Inst>(arg)) {
        if (inst->GetType(0) != *ty) {
          continue;
        }
        auto &&[clonedProg, cloned] = Clone(p, call);
        UnusedArgumentDeleter deleter(cloned);

        auto *ret = new ReturnInst(
            static_cast<Inst *>(*cloned->value_op_begin() + i),
            {}
        );
        cloned->getParent()->AddInst(ret, cloned);
        cloned->replaceAllUsesWith(ret);
        cloned->eraseFromParent();
        if (Verify(*clonedProg)) {
          return { { std::move(clonedProg), ret } };
        }
      }
    }

    {
      auto &&[clonedProg, cloned] = Clone(p, call);
      UnusedArgumentDeleter deleter(cloned);

      auto *undef = new UndefInst(*ty, {});
      cloned->getParent()->AddInst(undef, cloned);

      auto *ret = new ReturnInst(undef, {});
      cloned->getParent()->AddInst(ret, cloned);
      cloned->replaceAllUsesWith(ret);
      cloned->eraseFromParent();
      if (Verify(*clonedProg)) {
        return { { std::move(clonedProg), ret } };
      }
    }

    {
      auto &&[clonedProg, cloned] = Clone(p, call);
      UnusedArgumentDeleter deleter(cloned);

      auto *undef = new MovInst(*ty, GetZero(*ty), {});
      cloned->getParent()->AddInst(undef, cloned);

      auto *ret = new ReturnInst(undef, {});
      cloned->getParent()->AddInst(ret, cloned);
      cloned->replaceAllUsesWith(ret);
      cloned->eraseFromParent();
      if (Verify(*clonedProg)) {
        return { { std::move(clonedProg), ret } };
      }
    }

    return std::nullopt;
  } else {
    auto &&[clonedProg, clonedInst] = Clone(p, call);
    UnusedArgumentDeleter deleter(clonedInst);

    auto *ret = new ReturnInst({});
    clonedInst->getParent()->AddInst(ret, clonedInst);
    clonedInst->replaceAllUsesWith(ret);
    clonedInst->eraseFromParent();
    if (Verify(*clonedProg)) {
      return { { std::move(clonedProg), ret } };
    }
  }
  return std::nullopt;
}

// -----------------------------------------------------------------------------
InstReducerBase::It InstReducerBase::ReduceStore(Prog &p, StoreInst *i)
{
  if (It r = TryReducer(&InstReducerBase::ReduceErase, p, i)) {
    return r;
  }
  return std::nullopt;
}

// -----------------------------------------------------------------------------
InstReducerBase::It InstReducerBase::ReduceMov(Prog &p, MovInst *i)
{
  if (It r = TryReducer(&InstReducerBase::ReduceErase, p, i)) {
    return r;
  }
  if (It r = TryReducer(&InstReducerBase::ReduceToUndef, p, i)) {
    return r;
  }
  return ReduceToOp(p, i);
}

// -----------------------------------------------------------------------------
InstReducerBase::It InstReducerBase::ReduceArg(Prog &p, ArgInst *i)
{
  if (It r = TryReducer(&InstReducerBase::ReduceErase, p, i)) {
    return r;
  }
  if (It r = TryReducer(&InstReducerBase::ReduceToUndef, p, i)) {
    return r;
  }
  if (It r = TryReducer(&InstReducerBase::ReduceZero, p, i)) {
    return r;
  }
  return ReduceToOp(p, i);
}

// -----------------------------------------------------------------------------
InstReducerBase::It InstReducerBase::ReduceSwitch(Prog &p, SwitchInst *i)
{
  llvm_unreachable("missing reducer");
}

// -----------------------------------------------------------------------------
InstReducerBase::It InstReducerBase::ReduceJmp(Prog &p, JumpInst *i)
{
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

  RemoveUnreachable(from->getParent());

  if (Verify(*clonedProg)) {
    return { { std::move(clonedProg), trapInst } };
  }
  return std::nullopt;
}

// -----------------------------------------------------------------------------
InstReducerBase::It InstReducerBase::ReduceJcc(Prog &p, JumpCondInst *i)
{
  auto ToJump = [this, &p, i](bool flag) -> It {
    auto &&[clonedProg, cloned] = CloneT<JumpCondInst>(p, i);

    Block *from = cloned->getParent();
    Block *to = flag ? cloned->GetTrueTarget() : cloned->GetFalseTarget();
    Block *other = flag ? cloned->GetFalseTarget() : cloned->GetTrueTarget();
  
    JumpInst *jumpInst;
    {
      UnusedArgumentDeleter deleter(cloned);
      jumpInst = new JumpInst(to, cloned->GetAnnot());
      from->AddInst(jumpInst);
      cloned->eraseFromParent();
      RemoveEdge(from, other);
    }

    RemoveUnreachable(from->getParent());

    if (Verify(*clonedProg)) {
      return { { std::move(clonedProg), jumpInst } };
    }
    return std::nullopt;
  };

  if (auto It = ToJump(true)) {
    return It;
  }
  if (auto It = ToJump(false)) {
    return It;
  }
  return std::nullopt;
}

// -----------------------------------------------------------------------------
InstReducerBase::It InstReducerBase::ReduceRet(Prog &p, ReturnInst *i)
{
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
InstReducerBase::It InstReducerBase::ReducePhi(Prog &p, PhiInst *phi)
{
  // Prepare annotations for the new instructions.
  AnnotSet annot = phi->GetAnnot();
  annot.Clear(CAML_FRAME);
  annot.Clear(CAML_VALUE);

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

  {
    auto &&[clonedProg, clonedInst] = Clone(p, phi);
    UnusedArgumentDeleter deleter(clonedInst);

    auto *undef = new UndefInst(ty, annot);
    clonedInst->getParent()->AddInst(undef, GetInsertPoint(clonedInst));
    clonedInst->replaceAllUsesWith(undef);
    auto *next = &*std::next(clonedInst->getIterator());
    clonedInst->eraseFromParent();
    if (Verify(*clonedProg)) {
      return { { std::move(clonedProg), next } };
    }
  }

  {
    auto &&[clonedProg, clonedInst] = Clone(p, phi);
    UnusedArgumentDeleter deleter(clonedInst);

    auto *undef = new MovInst(ty, GetZero(ty), annot);
    clonedInst->getParent()->AddInst(undef, GetInsertPoint(clonedInst));
    clonedInst->replaceAllUsesWith(undef);
    auto *next = &*std::next(clonedInst->getIterator());
    clonedInst->eraseFromParent();
    if (Verify(*clonedProg)) {
      return { { std::move(clonedProg), next } };
    }
  }

  return std::nullopt;
}

// -----------------------------------------------------------------------------
InstReducerBase::It InstReducerBase::ReduceFNStCw(Prog &p, FNStCwInst *i)
{
  if (It r = TryReducer(&InstReducerBase::ReduceErase, p, i)) {
    return r;
  }
  return std::nullopt;
}

// -----------------------------------------------------------------------------
InstReducerBase::It InstReducerBase::ReduceUndef(Prog &p, UndefInst *i)
{
  if (It r = TryReducer(&InstReducerBase::ReduceErase, p, i)) {
    return r;
  }
  return std::nullopt;
}

// -----------------------------------------------------------------------------
Inst *InstReducerBase::ReduceTrap(Inst *inst)
{
  UnusedArgumentDeleter deleter(inst);

  auto *trap = new TrapInst(inst->GetAnnot());
  inst->getParent()->AddInst(trap, inst);
  inst->replaceAllUsesWith(trap);
  inst->eraseFromParent();
  return trap;
}

// -----------------------------------------------------------------------------
InstReducerBase::It InstReducerBase::ReduceOperator(Prog &p, Inst *i)
{
  if (It r = TryReducer(&InstReducerBase::ReduceErase, p, i)) {
    return r;
  }
  if (It r = TryReducer(&InstReducerBase::ReduceToUndef, p, i)) {
    return r;
  }
  if (It r = TryReducer(&InstReducerBase::ReduceZero, p, i)) {
    return r;
  }
  if (It r = TryReducer(&InstReducerBase::ReduceToArg, p, i)) {
    return r;
  }
  return ReduceToOp(p, i);
}

// -----------------------------------------------------------------------------
InstReducerBase::It InstReducerBase::ReduceToOp(Prog &p, Inst *inst)
{
  for (unsigned i = 0, n = inst->size(); i < n; ++i) {
    Value *value = *(inst->value_op_begin() + i);
    if (auto *op = ::dyn_cast_or_null<Inst>(value)) {
      if (inst->GetType(0) != op->GetType(0)) {
        continue;
      }

      // Clone & replace with arg.
      auto &&[clonedProg, clonedInst] = Clone(p, inst);
      UnusedArgumentDeleter deleter(clonedInst);

      auto *clonedOp = static_cast<Inst *>(*(clonedInst->value_op_begin() + i));
      auto *next = &*std::next(clonedInst->getIterator());
      clonedInst->replaceAllUsesWith(clonedOp);
      clonedInst->eraseFromParent();

      // Verify if cloned program works.
      if (Verify(*clonedProg)) {
        return { { std::move(clonedProg), next } };
      }
    }
  }
  return std::nullopt;
}

// -----------------------------------------------------------------------------
Inst *InstReducerBase::ReduceToUndef(Inst *inst)
{
  UnusedArgumentDeleter deleter(inst);

  AnnotSet annot = inst->GetAnnot();
  annot.Clear(CAML_FRAME);
  annot.Clear(CAML_VALUE);

  auto *undef = new UndefInst(inst->GetType(0), annot);
  inst->getParent()->AddInst(undef, inst);
  inst->replaceAllUsesWith(undef);
  inst->eraseFromParent();

  return undef;
}

// -----------------------------------------------------------------------------
Inst *InstReducerBase::ReduceZero(Inst *inst)
{
  UnusedArgumentDeleter deleter(inst);

  AnnotSet annot = inst->GetAnnot();
  annot.Clear(CAML_FRAME);
  annot.Clear(CAML_VALUE);

  Type type = inst->GetType(0);

  auto *mov = new MovInst(type, GetZero(type), annot);
  inst->getParent()->AddInst(mov, inst);
  inst->replaceAllUsesWith(mov);
  inst->eraseFromParent();
  return mov;
}

// -----------------------------------------------------------------------------
Inst *InstReducerBase::ReduceErase(Inst *inst)
{
  if (!inst->use_empty()) {
    return nullptr;
  }
  UnusedArgumentDeleter deleter(inst);
  auto *next = &*std::next(inst->getIterator());
  inst->eraseFromParent();
  return next;
}

// -----------------------------------------------------------------------------
Inst *InstReducerBase::ReduceToArg(Inst *inst)
{
  UnusedArgumentDeleter deleter(inst);

  Func *func = inst->getParent()->getParent();

  auto params = func->params();
  Type ty = inst->GetType(0);
  for (unsigned i = 0, n = params.size(); i < n; ++i) {
    if (params[i] == ty) {
      auto *arg = new ArgInst(ty, new ConstantInt(i), inst->GetAnnot());
      inst->getParent()->AddInst(arg, inst);
      inst->replaceAllUsesWith(arg);
      inst->eraseFromParent();
      return arg;
    }
  }
  return nullptr;
}

// -----------------------------------------------------------------------------
void InstReducerBase::RemoveEdge(Block *from, Block *to)
{
  for (PhiInst &phi : to->phis()) {
    phi.Remove(from);
  }
}

// -----------------------------------------------------------------------------
Constant *InstReducerBase::GetZero(Type type)
{
  switch (type) {
    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::I64:
    case Type::I128: {
      return new ConstantInt(0);
    }
    case Type::F32: case Type::F64: case Type::F80: {
      return new ConstantFloat(0.0f);
    }
  }
  llvm_unreachable("invalid type");
}
