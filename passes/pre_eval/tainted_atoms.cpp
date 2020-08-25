// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/func.h"
#include "core/block.h"
#include "core/insts.h"
#include "core/atom.h"
#include "passes/pre_eval/tainted_atoms.h"



// -----------------------------------------------------------------------------
bool TaintedAtoms::Tainted::Union(const Tainted &that)
{
  if (all_ || that.all_) {
    bool changed = !all_;
    all_ = true;
    atoms_.clear();
    return changed;
  }

  if (atoms_.empty() && that.atoms_.empty()) {
    return false;
  }

  bool changed = false;
  for (Atom *atom : that.atoms_) {
    changed |= atoms_.insert(atom).second;
  }
  return changed;
}

// -----------------------------------------------------------------------------
bool TaintedAtoms::Tainted::Add(Atom *atom)
{
  if (all_) {
    return false;
  }
  return atoms_.insert(atom).second;
}

// -----------------------------------------------------------------------------
TaintedAtoms::TaintedAtoms(const Func &func)
{
  if (func.HasAddressTaken()) {
    Visit(func.getEntryBlock(), Tainted(Tainted::All{}));
  } else {
    Visit(func.getEntryBlock(), Tainted());
  }
}

// -----------------------------------------------------------------------------
TaintedAtoms::~TaintedAtoms()
{
}

// -----------------------------------------------------------------------------
const TaintedAtoms::Tainted *TaintedAtoms::operator[](const Block &block) const
{
  auto it = blocks_.find(&block);
  if (it == blocks_.end()) {
    return nullptr;
  } else {
    return &it->second->Entry;
  }
}

// -----------------------------------------------------------------------------
void TaintedAtoms::Visit(const Block &block, const Tainted &vals)
{
  // Create entry & exit sets. Only visit when new information does not
  // change entry or this is the first time the block is visited.
  Tainted *entry, *exit;
  {
    auto it = blocks_.emplace(&block, nullptr);
    if (it.second) {
      it.first->second = std::make_unique<BlockInfo>(vals, Tainted{});
    } else {
      if (!it.first->second->Entry.Union(vals)) {
        return;
      }
    }
    entry = &it.first->second->Entry;
    exit = &it.first->second->Exit;
  }

  // Order doesn't really matter, so propagate information now.
  exit->Union(*entry);

  for (const Inst &inst : block) {
    switch (inst.GetKind()) {
      case Inst::Kind::CALL: {
        llvm_unreachable("CALL");
        continue;
      }
      case Inst::Kind::TCALL: {
        llvm_unreachable("TCALL");
        continue;
      }
      case Inst::Kind::INVOKE: {
        llvm_unreachable("INVOKE");
        continue;
      }
      case Inst::Kind::TINVOKE: {
        llvm_unreachable("TINVOKE");
        continue;
      }
      case Inst::Kind::RET: {
        Exit(*block.getParent()).Union(*exit);
        return;
      }
      case Inst::Kind::JCC: {
        llvm_unreachable("JCC");
        continue;
      }
      case Inst::Kind::JI: {
        llvm_unreachable("JI");
        continue;
      }
      case Inst::Kind::JMP: {
        auto &jmp = static_cast<const JumpInst &>(inst);
        return Visit(*jmp.GetTarget(), *exit);
      }
      case Inst::Kind::SWITCH: {
        llvm_unreachable("SWITCH");
        continue;
      }
      case Inst::Kind::TRAP: {
        llvm_unreachable("TRAP");
        continue;
      }
      case Inst::Kind::SYSCALL: {
        llvm_unreachable("SYSCALL");
        continue;
      }
      case Inst::Kind::SET: {
        llvm_unreachable("SET");
        continue;
      }
      // Mov introduces new atoms.
      case Inst::Kind::MOV: {
        auto *arg = static_cast<const MovInst &>(inst).GetArg();
        switch (arg->GetKind()) {
          case Value::Kind::CONST:
          case Value::Kind::INST: {
            continue;
          }
          case Value::Kind::GLOBAL: {
            switch (static_cast<Global *>(arg)->GetKind()) {
              case Global::Kind::EXTERN:
              case Global::Kind::FUNC:
              case Global::Kind::BLOCK: {
                continue;
              }
              case Global::Kind::ATOM: {
                exit->Add(static_cast<Atom *>(arg));
                continue;
              }
            }
            llvm_unreachable("invalid global kind");
          }
          case Value::Kind::EXPR: {
            switch (static_cast<Expr *>(arg)->GetKind()) {
              case Expr::Kind::SYMBOL_OFFSET: {
                auto *sym = static_cast<SymbolOffsetExpr *>(arg)->GetSymbol();
                switch (sym->GetKind()) {
                  case Global::Kind::EXTERN:
                  case Global::Kind::FUNC:
                  case Global::Kind::BLOCK: {
                    continue;
                  }
                  case Global::Kind::ATOM: {
                    exit->Add(static_cast<Atom *>(sym));
                    continue;
                  }
                }
                llvm_unreachable("not implemented");
              }
            }
            llvm_unreachable("invalid expression kind");
          }
        }
        llvm_unreachable("invalid value kind");
      }
      case Inst::Kind::LD:
      case Inst::Kind::ST:
      case Inst::Kind::XCHG:
      case Inst::Kind::CMPXCHG:
      case Inst::Kind::VASTART:
      case Inst::Kind::ALLOCA:
      case Inst::Kind::ARG:
      case Inst::Kind::FRAME:
      case Inst::Kind::UNDEF:
      case Inst::Kind::RDTSC:
      case Inst::Kind::FNSTCW:
      case Inst::Kind::FLDCW:
      case Inst::Kind::SELECT:
      case Inst::Kind::ABS:
      case Inst::Kind::NEG:
      case Inst::Kind::SQRT:
      case Inst::Kind::SIN:
      case Inst::Kind::COS:
      case Inst::Kind::SEXT:
      case Inst::Kind::ZEXT:
      case Inst::Kind::FEXT:
      case Inst::Kind::XEXT:
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
      case Inst::Kind::ADD:
      case Inst::Kind::AND:
      case Inst::Kind::CMP:
      case Inst::Kind::UDIV:
      case Inst::Kind::UREM:
      case Inst::Kind::SDIV:
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
      case Inst::Kind::PHI: {
        // No new constants introduced.
        continue;
      }
    }
  }

  llvm_unreachable("missing terminator");
}

// -----------------------------------------------------------------------------
TaintedAtoms::Tainted &TaintedAtoms::Exit(const Func &func)
{
  auto it = exits_.emplace(&func, nullptr);
  if (it.second) {
    it.first->second = std::make_unique<Tainted>();
  }
  return *it.first->second;
}
