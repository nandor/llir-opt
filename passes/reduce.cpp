// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <sstream>
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/Support/RandomNumberGenerator.h>

#include "core/atom.h"
#include "core/block.h"
#include "core/cast.h"
#include "core/cfg.h"
#include "core/data.h"
#include "core/func.h"
#include "core/prog.h"
#include "core/insts.h"
#include "passes/reduce.h"



// -----------------------------------------------------------------------------
template <typename T, typename Gen>
T PickOne(const std::vector<T> &items, Gen &gen)
{
  return items[(std::uniform_int_distribution<>(0, items.size() - 1))(gen)];
}

// -----------------------------------------------------------------------------
template <typename T>
void ReducePass::RemoveArg(T *i)
{
  // Pick a random argument.
  std::vector<Inst *> args;
  for (Inst *arg : i->args()) {
    args.push_back(arg);
  }
  if (args.empty()) {
    return;
  }
  args.erase(args.begin() + Random(args.size() - 1));

  T *inst = new T(
    i->GetType(),
    i->GetCallee(),
    args,
    i->GetNumFixedArgs(),
    i->GetCallingConv(),
    i->GetAnnot()
  );
  i->getParent()->AddInst(inst, i);
  i->replaceAllUsesWith(inst);
  i->eraseFromParent();
}

// -----------------------------------------------------------------------------
void ReducePass::Run(Prog *prog)
{
  Reduce(prog);
}

// -----------------------------------------------------------------------------
void ReducePass::Reduce(Prog *prog)
{
  // Pick a data item to work on.
  switch (auto action = Random(6)) {
    case 0: case 1: case 2: {
      // Pick a function to work on.
      std::vector<Func *> nonEmptyFuncs, emptyFuncs;
      for (Func &f : *prog) {
        if (f.size() == 1 && f.begin()->size() == 1) {
          emptyFuncs.push_back(&f);
        } else {
          for (Block &b : f) {
            nonEmptyFuncs.push_back(&f);
          }
        }
      }

      switch (action) {
        case 0: {
          if (nonEmptyFuncs.empty()) {
            return;
          }
          Func *f = PickOne(nonEmptyFuncs, rand_);
          auto *bb = new Block((".L" + f->getName() + "_entry").str());
          f->AddBlock(bb);
          bb->AddInst(new TrapInst({}));
          return;
        }
        case 1: {
          if (emptyFuncs.empty()) {
            return;
          }
          Func *f = PickOne(emptyFuncs, rand_);
          for (Use &use : f->uses()) {
            if (auto *expr = ::dyn_cast_or_null<Expr>(use.getUser())) {
              switch (expr->GetKind()) {
                case Expr::Kind::SYMBOL_OFFSET: {
                  use = nullptr;
                  break;
                }
              }
            }
          }
          return;
        }
        case 2: {
          if (emptyFuncs.empty()) {
            return;
          }
          Func *f = PickOne(emptyFuncs, rand_);
          std::ostringstream os;
          os << f->GetName() << "$$extern_dummy";
          Global *ext = prog->GetGlobal(os.str());
          f->replaceAllUsesWith(ext);
          f->eraseFromParent();
          return;
        }
      }
      llvm_unreachable("missing reducer");
      break;
    }
    case 3: {
      std::vector<Inst *> insts;
      for (Func &func : *prog) {
        for (Block &block : func) {
          for (Inst &inst : block) {
            switch (inst.GetKind()) {
              case Inst::Kind::TRAP:
              case Inst::Kind::UNDEF:
                continue;
              default:
                break;
            }
            insts.push_back(&inst);
          }
        }
      }
      if (insts.empty()) {
        return;
      }
      ReduceInst(PickOne(insts, rand_));
      return;
    }
    case 4: {
      // Erase a data item.
      std::vector<Atom *> atoms;
      for (Data &data : prog->data()) {
        for (Atom &atom : data) {
          atoms.push_back(&atom);
          for (Item &item : atom) {
            atoms.push_back(&atom);
          }
        }
      }
      if (atoms.empty()) {
        return;
      }

      Atom *atom = PickOne(atoms, rand_);
      Global *ext = prog->GetGlobal("$$$extern_dummy");
      atom->replaceAllUsesWith(ext);
      atom->eraseFromParent();
      return;
    }
    case 5: {
      // Erase a data item.
      std::vector<Item *> items;
      for (Data &data : prog->data()) {
        for (Atom &atom : data) {
          for (Item &item : atom) {
            items.push_back(&item);
          }
        }
      }
      if (items.empty()) {
        return;
      }
      Item *item = PickOne(items, rand_);
      item->eraseFromParent();
      return;
    }
    case 6: {
      // Erase a block.
      std::vector<Block *> blocks;
      for (Func &f : *prog) {
        for (Block &b : f) {
          if (b.size() == 1 && b.GetTerminator()->Is(Inst::Kind::TRAP)) {
            continue;
          }
          blocks.push_back(&b);
        }
      }
      if (blocks.empty()) {
        return;
      }

      Block *block = PickOne(blocks, rand_);
      for (Block *succ : block->successors()) {
        for (PhiInst &phi : succ->phis()) {
          if (phi.HasValue(block)) {
            phi.Remove(block);
          }
        }
      }
      block->clear();
      block->AddInst(new TrapInst({}));
      RemoveUnreachable(block->getParent());
      return;
    }
  }
  llvm_unreachable("missing reducer");
}

// -----------------------------------------------------------------------------
void ReducePass::ReduceInst(Inst *i)
{
  // Mutate the instruction based on its kind.
  switch (i->GetKind()) {
    case Inst::Kind::CALL:      return ReduceCall(static_cast<CallInst *>(i));
    case Inst::Kind::TCALL:     return ReduceTailCall(static_cast<TailCallInst *>(i));
    case Inst::Kind::INVOKE:    return ReduceInvoke(static_cast<InvokeInst *>(i));
    case Inst::Kind::TINVOKE:   llvm_unreachable("TINVOKE");
    case Inst::Kind::RET:       return ReduceRet(static_cast<ReturnInst *>(i));
    case Inst::Kind::JCC:       return ReduceJcc(static_cast<JumpCondInst *>(i));
    case Inst::Kind::JI:        llvm_unreachable("JI");
    case Inst::Kind::JMP:       return ReduceJmp(static_cast<JumpInst *>(i));
    case Inst::Kind::SWITCH:    return ReduceSwitch(static_cast<SwitchInst *>(i));
    case Inst::Kind::TRAP:      return;
    case Inst::Kind::LD:        return ReduceLoad(static_cast<LoadInst *>(i));
    case Inst::Kind::ST:        return ReduceStore(static_cast<StoreInst *>(i));
    case Inst::Kind::XCHG:      llvm_unreachable("XCHG");
    case Inst::Kind::SET:       llvm_unreachable("SET");
    case Inst::Kind::VASTART:   llvm_unreachable("VASTART");
    case Inst::Kind::ALLOCA:    llvm_unreachable("ALLOCA");
    case Inst::Kind::ARG:       return ReduceArg(static_cast<ArgInst *>(i));
    case Inst::Kind::FRAME:     return ReduceFrame(static_cast<FrameInst *>(i));
    case Inst::Kind::UNDEF:     return;
    case Inst::Kind::RDTSC:     llvm_unreachable("RDTSC");
    case Inst::Kind::FNSTCW:    return ReduceFNStCw(static_cast<FNStCwInst *>(i));
    case Inst::Kind::FLDCW:     return ReduceFLdCw(static_cast<FLdCwInst *>(i));
    case Inst::Kind::MOV:       return ReduceMov(static_cast<MovInst *>(i));
    case Inst::Kind::SELECT:    return ReduceSelect(static_cast<SelectInst *>(i));
    case Inst::Kind::PHI:       return ReducePhi(static_cast<PhiInst *>(i));

    case Inst::Kind::ABS:
    case Inst::Kind::NEG:
    case Inst::Kind::SQRT:
    case Inst::Kind::SIN:
    case Inst::Kind::COS:
    case Inst::Kind::SEXT:
    case Inst::Kind::ZEXT:
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
      return ReduceUnary(static_cast<UnaryInst *>(i));

    case Inst::Kind::ADD:
    case Inst::Kind::AND:
    case Inst::Kind::CMP:
    case Inst::Kind::DIV:
    case Inst::Kind::REM:
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
      return ReduceBinary(static_cast<BinaryInst *>(i));
  }
}

// -----------------------------------------------------------------------------
void ReducePass::ReduceArg(ArgInst *i)
{
  switch (Random(0)) {
    case 0: return ReduceUndefined(i);
  }
  llvm_unreachable("missing reducer");
}

// -----------------------------------------------------------------------------
void ReducePass::ReduceFrame(FrameInst *i)
{
  switch (Random(0)) {
    case 0: return ReduceUndefined(i);
  }
  llvm_unreachable("missing reducer");
}

// -----------------------------------------------------------------------------
void ReducePass::ReduceCall(CallInst *i)
{
  if (auto ty = i->GetType()) {
    switch (Random(1)) {
      case 0: return ReduceUndefined(i);
      case 1: return RemoveArg<CallInst>(i);
    }
  } else {
    switch (Random(1)) {
      case 0: return ReduceErase(i);
      case 1: return RemoveArg<CallInst>(i);
    }
  }
  llvm_unreachable("missing reducer");
}

// -----------------------------------------------------------------------------
void ReducePass::ReduceInvoke(InvokeInst *i)
{
  if (auto ty = i->GetType()) {
    switch (Random(0)) {
      case 0: {
        auto *block = i->getParent();
        auto *branch = Random(1) ? i->GetCont() : i->GetThrow();
        ReduceUndefined(i);
        auto *jump = new JumpInst(branch, {});
        block->AddInst(jump);
        return;
      }
    }
  } else {
    llvm_unreachable("missing reducer");
  }
  llvm_unreachable("missing reducer");
}

// -----------------------------------------------------------------------------
void ReducePass::ReduceTailCall(TailCallInst *i)
{
  if (auto ty = i->GetType()) {
    switch (Random(1)) {
      case 0: {
        auto *trap = new TrapInst({});
        i->getParent()->AddInst(trap, i);
        i->replaceAllUsesWith(trap);
        i->eraseFromParent();
        return;
      }
      case 1: return RemoveArg<TailCallInst>(i);
    }
  } else {
    switch (Random(2)) {
      case 0: {
        auto *trap = new TrapInst({});
        i->getParent()->AddInst(trap, i);
        i->replaceAllUsesWith(trap);
        i->eraseFromParent();
        return;
      }
      case 1: {
        auto *ret = new ReturnInst({});
        i->getParent()->AddInst(ret, i);
        i->replaceAllUsesWith(ret);
        i->eraseFromParent();
        return;
      }
      case 2: return RemoveArg<TailCallInst>(i);
    }
  }
  llvm_unreachable("missing reducer");
}

// -----------------------------------------------------------------------------
void ReducePass::ReduceLoad(LoadInst *i)
{
  switch (Random(0)) {
    case 0: return ReduceUndefined(i);
  }
  llvm_unreachable("missing reducer");
}

// -----------------------------------------------------------------------------
void ReducePass::ReduceStore(StoreInst *i)
{
  switch (Random(0)) {
    case 0: return ReduceErase(i);
  }
  llvm_unreachable("missing reducer");
}

// -----------------------------------------------------------------------------
void ReducePass::ReduceMov(MovInst *i)
{
  switch (Random(0)) {
    case 0: return ReduceUndefined(i);
  }
  llvm_unreachable("missing reducer");
}

// -----------------------------------------------------------------------------
void ReducePass::ReduceUnary(UnaryInst *i)
{
  switch (Random(0)) {
    case 0: return ReduceUndefined(i);
  }
  llvm_unreachable("missing reducer");
}

// -----------------------------------------------------------------------------
void ReducePass::ReduceBinary(BinaryInst *i)
{
  switch (Random(0)) {
    case 0: return ReduceUndefined(i);
  }
  llvm_unreachable("missing reducer");
}

// -----------------------------------------------------------------------------
void ReducePass::ReduceSwitch(SwitchInst *i)
{
  unsigned n = i->getNumSuccessors();
  assert(n > 0 && "invalid switch");

  Block *from = i->getParent();
  if (n == 1) {
    Block *to = i->getSuccessor(0);
    from->AddInst(new TrapInst({}), i);
    i->eraseFromParent();
    RemoveEdge(from, to);
  } else {
    std::vector<Block *> branches;
    Block *to = nullptr;
    unsigned index = Random(n - 1);
    for (unsigned j = 0; j < n; ++j) {
      Block *succ = i->getSuccessor(j);
      if (j != index) {
        branches.push_back(succ);
      } else {
        assert(!to);
        to = succ;
      }
    }
    assert(to && "missing branch to delete");
    from->AddInst(new SwitchInst(i->GetIdx(), branches, i->GetAnnot()));
    i->eraseFromParent();
    RemoveEdge(from, to);
  }
}

// -----------------------------------------------------------------------------
void ReducePass::ReduceJmp(JumpInst *i)
{
  Block *from = i->getParent();
  Block *to = i->GetTarget();
  from->AddInst(new TrapInst({}), i);
  i->eraseFromParent();
  RemoveEdge(from, to);
}

// -----------------------------------------------------------------------------
void ReducePass::ReduceJcc(JumpCondInst *i)
{
  bool flag = Random(1);

  Block *from = i->getParent();
  Block *to = flag ? i->GetTrueTarget() : i->GetFalseTarget();
  Block *other = flag ? i->GetFalseTarget() : i->GetTrueTarget();

  from->AddInst(new JumpInst(to, i->GetAnnot()), i);
  i->eraseFromParent();
  RemoveEdge(from, other);
}

// -----------------------------------------------------------------------------
void ReducePass::ReduceRet(ReturnInst *i)
{
  i->getParent()->AddInst(new TrapInst({}), i);
  i->eraseFromParent();
}

// -----------------------------------------------------------------------------
void ReducePass::ReducePhi(PhiInst *phi)
{
  unsigned n = phi->GetNumIncoming();
  unsigned i = Random(n);
  if (i == n) {
    Block *parent = phi->getParent();
    auto it = parent->begin();
    while (it != parent->end() && it->Is(Inst::Kind::PHI))
      ++it;

    auto *undef = new UndefInst(phi->GetType(0), phi->GetAnnot());
    parent->AddInst(undef, &*it);
    phi->replaceAllUsesWith(undef);
    phi->eraseFromParent();
  } else {
    Constant *cst = nullptr;
    switch (phi->GetType(0)) {
      case Type::I8:
      case Type::I16:
      case Type::I32:
      case Type::I64:
      case Type::I128:
      case Type::U8:
      case Type::U16:
      case Type::U32:
      case Type::U64:
      case Type::U128: {
        phi->SetValue(i, new ConstantInt(0));
        return;
      }
      case Type::F32:
      case Type::F64:
      case Type::F80: {
        phi->SetValue(i, new ConstantFloat(0.0));
        return;
      }
    }
    llvm_unreachable("invalid phi type");
  }
}

// -----------------------------------------------------------------------------
void ReducePass::ReduceFNStCw(FNStCwInst *i)
{
  switch (Random(0)) {
    case 0: return ReduceErase(i);
  }
  llvm_unreachable("missing reducer");
}

// -----------------------------------------------------------------------------
void ReducePass::ReduceFLdCw(FLdCwInst *i)
{
  switch (Random(0)) {
    case 0: return ReduceErase(i);
  }
  llvm_unreachable("missing reducer");
}


// -----------------------------------------------------------------------------
void ReducePass::ReduceUndefined(Inst *i)
{
  AnnotSet annot = i->GetAnnot();
  annot.Clear(CAML_FRAME);
  annot.Clear(CAML_VALUE);

  auto *undef = new UndefInst(i->GetType(0), annot);
  i->getParent()->AddInst(undef, i);
  i->replaceAllUsesWith(undef);
  i->eraseFromParent();
}

// -----------------------------------------------------------------------------
void ReducePass::ReduceErase(Inst *i)
{
  i->eraseFromParent();
}

// -----------------------------------------------------------------------------
void ReducePass::RemoveEdge(Block *from, Block *to)
{
  for (PhiInst &phi : to->phis()) {
    phi.Remove(from);
  }
}

// -----------------------------------------------------------------------------
void ReducePass::ReduceSelect(SelectInst *select)
{
  Inst *arg = nullptr;
  switch (Random(2)) {
    case 0: return ReduceUndefined(select);
    case 1: {
      arg = select->GetTrue();
      break;
    }
    case 2: {
      arg = select->GetFalse();
      break;
    }
  }

  select->replaceAllUsesWith(arg);
  select->eraseFromParent();
}

// -----------------------------------------------------------------------------
unsigned ReducePass::Random(unsigned n)
{
  return (std::uniform_int_distribution<>(0, n))(rand_);
}

// -----------------------------------------------------------------------------
const char *ReducePass::GetPassName() const
{
  return "Test Reducer";
}
