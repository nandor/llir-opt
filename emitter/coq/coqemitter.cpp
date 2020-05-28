// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/PostOrderIterator.h>

#include "core/cast.h"
#include "core/cfg.h"
#include "core/func.h"
#include "core/prog.h"
#include "core/analysis/dominator.h"
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
  os_ << "Require Import LLIR.Values.\n";
  os_ << "Require Import LLIR.Verify.\n";
  os_ << "Require Import LLIR.State.\n";
  os_ << "Require Import LLIR.Export.\n";
  os_ << "Require Import LLIR.Dom.\n";
  os_ << "Require Import Coq.Lists.List.\n";
  os_ << "Import ListNotations.\n";

  os_ << "\n";
  for (const Func &func : prog) {
    WriteDefinition(func);
    WriteInversion(func);
    WriteDefinedAtInversion(func);
    WriteUsedAtInversion(func);
    WriteUsesHaveDefs(func);
    WriteBlocks(func);
    WriteDominators(func);

    insts_.clear();
    blocks_.clear();
  }
}

// -----------------------------------------------------------------------------
void CoqEmitter::Write(std::string_view name)
{
  for (char chr : name) {
    if (chr == '$') {
      os_ << "__";
    } else {
      os_ << chr;
    }
  }
}

// -----------------------------------------------------------------------------
void CoqEmitter::WriteDefinition(const Func &func)
{
  os_ << "Definition ";
  Write(func.GetName());
  os_ << ": func := \n";

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
  for (const Block *block : blockOrder) {
    std::optional<unsigned> firstNonPhi;
    for (const Inst &inst : *block) {
      unsigned idx = insts_.size() + 1;
      insts_[&inst] = idx;
      if (!inst.Is(Inst::Kind::PHI) && !firstNonPhi) {
        firstNonPhi = idx;
      }
    }
    assert(firstNonPhi && "empty block");
    blocks_[block] = *firstNonPhi;
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

        unsigned idx = insts_[&*it];
        if (entry) {
          os_ << ";  ";
        } else {
          entry = idx;
        }

        os_ << "(" << idx << "%positive, ";
        Write(it);
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

        os_ << " (" << blocks_[block] << "%positive\n";
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
            const Inst *value = phi->GetValue(j);
            os_ << "(";
            os_ << insts_[block->GetTerminator()] << "%positive";
            os_ << ", ";
            os_ << insts_[value] << "%positive";
            os_ << ")\n";
            os_.indent(11);
          }
          os_ << "]\n";
          os_.indent(11) << insts_[phi] << "%positive\n";
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

// -----------------------------------------------------------------------------
void CoqEmitter::Write(Block::const_iterator it)
{
  switch (it->GetKind()) {
    case Inst::Kind::CALL: llvm_unreachable("CALL");
    case Inst::Kind::TCALL: llvm_unreachable("TCALL");
    case Inst::Kind::INVOKE: llvm_unreachable("INVOKE");
    case Inst::Kind::TINVOKE: llvm_unreachable("TINVOKE");
    case Inst::Kind::RET: {
      auto &inst = static_cast<const ReturnInst &>(*it);
      if (auto *val = inst.GetValue()) {
        os_ << "LLRet " << insts_[val] << "%positive";
      } else {
        os_ << "LLRetVoid";
      }
      return;
    }
    case Inst::Kind::JCC: {
      auto &inst = static_cast<const JumpCondInst &>(*it);
      os_ << "LLJcc ";
      os_ << insts_[inst.GetCond()] << "%positive ";
      os_ << blocks_[inst.GetTrueTarget()] << "%positive ";
      os_ << blocks_[inst.GetFalseTarget()] << "%positive";
      return;
    }
    case Inst::Kind::JI: llvm_unreachable("JI");
    case Inst::Kind::JMP: {
      auto &inst = static_cast<const JumpInst &>(*it);
      os_ << "LLJmp " << blocks_[inst.GetTarget()] << "%positive";
      return;
    }
    case Inst::Kind::SWITCH: llvm_unreachable("SWITCH");
    case Inst::Kind::TRAP: llvm_unreachable("TRAP");
    // Load.
    case Inst::Kind::LD: llvm_unreachable("LD");
    // Store.
    case Inst::Kind::ST: llvm_unreachable("ST");
    // Atomic exchange.
    case Inst::Kind::XCHG: llvm_unreachable("XCHG");
    // Atomic compare and exchange.
    case Inst::Kind::CMPXCHG: llvm_unreachable("CMPXCHG");
    // Set register.
    case Inst::Kind::SET: llvm_unreachable("SET");
    // Variable argument lists.
    case Inst::Kind::VASTART: llvm_unreachable("VASTART");
    // Dynamic stack allcoation.
    case Inst::Kind::ALLOCA: llvm_unreachable("ALLOCA");
    // Argument.
    case Inst::Kind::ARG: {
      auto &inst = static_cast<const ArgInst &>(*it);
      os_ << "LLArg " << inst.GetIdx() << " ";
      os_ << insts_[&inst] << "%positive ";
      os_ << insts_[&*std::next(it)] << "%positive";
      return;
    }
    // Frame address.
    case Inst::Kind::FRAME: llvm_unreachable("FRAME");
    // Undefined value.
    case Inst::Kind::UNDEF: {
      auto &inst = static_cast<const UndefInst &>(*it);
      os_ << "LLUndef ";
      Write(inst.GetType());
      os_ << " ";
      os_ << insts_[&inst] << "%positive ";
      os_ << insts_[&*std::next(it)] << "%positive";
      return;
    }
    // Hardware instructions.
    case Inst::Kind::RDTSC: llvm_unreachable("RDTSC");
    case Inst::Kind::FNSTCW: llvm_unreachable("FNSTCW");
    case Inst::Kind::FLDCW: llvm_unreachable("FLDCW");
    case Inst::Kind::SYSCALL: llvm_unreachable("SYSCALL");
    // Conditional.
    case Inst::Kind::SELECT: llvm_unreachable("SELECT");
    // PHI node.
    case Inst::Kind::PHI: llvm_unreachable("PHI");
    case Inst::Kind::MOV:       return Mov(it);
    // Unary instructions.
    case Inst::Kind::ABS:       return Unary(it, "Abs");
    case Inst::Kind::NEG:       return Unary(it, "Neg");
    case Inst::Kind::SQRT:      return Unary(it, "Sqrt");
    case Inst::Kind::SIN:       return Unary(it, "Sin");
    case Inst::Kind::COS:       return Unary(it, "Cos");
    case Inst::Kind::SEXT:      return Unary(it, "Sext");
    case Inst::Kind::ZEXT:      return Unary(it, "Zext");
    case Inst::Kind::FEXT:      return Unary(it, "Fext");
    case Inst::Kind::XEXT:      return Unary(it, "Xext");
    case Inst::Kind::TRUNC:     return Unary(it, "Trunc");
    case Inst::Kind::EXP:       return Unary(it, "Exp");
    case Inst::Kind::EXP2:      return Unary(it, "Exp2");
    case Inst::Kind::LOG:       return Unary(it, "Log");
    case Inst::Kind::LOG2:      return Unary(it, "Log2");
    case Inst::Kind::LOG10:     return Unary(it, "LOG10");
    case Inst::Kind::FCEIL:     return Unary(it, "Fceil");
    case Inst::Kind::FFLOOR:    return Unary(it, "Ffloor");
    case Inst::Kind::POPCNT:    return Unary(it, "Popcnt");
    case Inst::Kind::CLZ:       return Unary(it, "Clz");
    case Inst::Kind::CTZ:       return Unary(it, "Ctz");
    // Binary instructions
    case Inst::Kind::ADD:       return Binary(it, "Add");
    case Inst::Kind::AND:       return Binary(it, "And");
    case Inst::Kind::CMP:       return Binary(it, "Cmp");
    case Inst::Kind::UDIV:      return Binary(it, "UDiv");
    case Inst::Kind::UREM:      return Binary(it, "URem");
    case Inst::Kind::SDIV:      return Binary(it, "SDiv");
    case Inst::Kind::SREM:      return Binary(it, "SRem");
    case Inst::Kind::MUL:       return Binary(it, "Mul");
    case Inst::Kind::OR:        return Binary(it, "Or");
    case Inst::Kind::ROTL:      return Binary(it, "Rotl");
    case Inst::Kind::ROTR:      return Binary(it, "Rotr");
    case Inst::Kind::SLL:       return Binary(it, "Sll");
    case Inst::Kind::SRA:       return Binary(it, "Sra");
    case Inst::Kind::SRL:       return Binary(it, "Srl");
    case Inst::Kind::SUB:       return Binary(it, "Sub");
    case Inst::Kind::XOR:       return Binary(it, "Xor");
    case Inst::Kind::POW:       return Binary(it, "Pow");
    case Inst::Kind::COPYSIGN:  return Binary(it, "Copysign");
    case Inst::Kind::UADDO:     return Binary(it, "UAddO");
    case Inst::Kind::UMULO:     return Binary(it, "UMulO");
    case Inst::Kind::USUBO:     return Binary(it, "USubO");
    case Inst::Kind::SADDO:     return Binary(it, "SAddO");
    case Inst::Kind::SMULO:     return Binary(it, "SMulO");
    case Inst::Kind::SSUBO:     return Binary(it, "SSubO");
  }
  llvm_unreachable("invalid instruction kind");
}

// -----------------------------------------------------------------------------
void CoqEmitter::Unary(Block::const_iterator it, const char *op)
{
  auto &unary = static_cast<const UnaryInst &>(*it);

  os_ << "LLUnop ";
  Write(unary.GetType());
  os_ << " LL" << op << " ";
  os_ << insts_[unary.GetArg()] << "%positive ";
  os_ << insts_[&unary] << "%positive ";
  os_ << insts_[&*std::next(it)] << "%positive";
}

// -----------------------------------------------------------------------------
void CoqEmitter::Binary(Block::const_iterator it, const char *op)
{
  auto &binary = static_cast<const BinaryInst &>(*it);

  os_ << "LLBinop ";
  Write(binary.GetType());
  os_ << " LL" << op << " ";
  os_ << insts_[binary.GetLHS()] << "%positive ";
  os_ << insts_[binary.GetRHS()] << "%positive ";
  os_ << insts_[&binary] << "%positive ";
  os_ << insts_[&*std::next(it)] << "%positive";
}

// -----------------------------------------------------------------------------
template<unsigned Bits>
void Int(llvm::raw_ostream &os, const APInt &val)
{
  constexpr unsigned Half = Bits / 2;
  const APInt &lo = val.extractBits(Half, 0);
  const APInt &hi = val.extractBits(Half, Half);
  os << "(";
  Int<Half>(os, hi);
  os << ", ";
  Int<Half>(os, lo);
  os << ")";
}

template<>
void Int<1>(llvm::raw_ostream &os, const APInt &val)
{
  os << (val.isNullValue() ? "O" : "I");
}

// -----------------------------------------------------------------------------
template<unsigned Bits>
void CoqEmitter::MovInt(
    Block::const_iterator it,
    const char *op,
    const APInt &val)
{
  os_ << op << " ";
  Int<Bits>(os_, val);
  os_ << " ";
  os_ << insts_[&*it] << "%positive ";
  os_ << insts_[&*std::next(it)] << "%positive";
}

// -----------------------------------------------------------------------------
void CoqEmitter::Mov(Block::const_iterator it)
{
  auto &inst = static_cast<const MovInst &>(*it);
  auto *arg = inst.GetArg();
  switch (arg->GetKind()) {
    case Value::Kind::INST: llvm_unreachable("INST");
    case Value::Kind::GLOBAL: llvm_unreachable("GLOBAL");
    case Value::Kind::EXPR: llvm_unreachable("EXPR");
    case Value::Kind::CONST: {
      switch (static_cast<const Constant *>(arg)->GetKind()) {
        case Constant::Kind::INT: {
          const APInt &val = static_cast<const ConstantInt *>(arg)->GetValue();
          switch (inst.GetType()) {
            case Type::I8: return MovInt<8>(it, "LLInt8", val);
            case Type::I16: return MovInt<16>(it, "LLInt16", val);
            case Type::I32: return MovInt<32>(it, "LLInt32", val);
            case Type::I64: return MovInt<64>(it, "LLInt64", val);
            case Type::I128: return MovInt<128>(it, "LLInt128", val);

            case Type::F32:
            case Type::F64:
            case Type::F80: {
              llvm_unreachable("FLOAT");
            }
          }
          llvm_unreachable("invalid instruction type");
        }
        case Constant::Kind::FLOAT: {
          llvm_unreachable("FLOAT");
        }
        case Constant::Kind::REG: {
          llvm_unreachable("REG");
        }
      }
      llvm_unreachable("invalid constant kind");
    }
  }
  llvm_unreachable("invalid value kind");
}

// -----------------------------------------------------------------------------
void CoqEmitter::WriteInversion(const Func &func)
{
  os_ << "Theorem "; Write(func.GetName()); os_ << "_inversion:\n";
  os_.indent(2) << "forall (inst: option inst) (n: node),\n";
  os_.indent(2) << "(fn_insts "; Write(func.GetName()); os_ << ") ! n = inst ->\n";
  for (const Block &block : func) {
    for (auto it = block.begin(), end = block.end(); it != end; ++it) {
      if (it->Is(Inst::Kind::PHI)) {
        continue;
      }
      os_.indent(4);
      os_ << "(" << insts_[&*it] << "%positive = n /\\ Some (";
      Write(it);
      os_ << ") = inst)\n";
      os_.indent(4);
      os_ << "\\/\n";
    }
  }
  os_.indent(4) << "inst = None.\n";
  os_ << "Proof. inversion_proof "; Write(func.GetName()); os_ << ". Qed.\n\n";
}

// -----------------------------------------------------------------------------
void CoqEmitter::WriteDefinedAtInversion(const Func &func)
{
  os_ << "Theorem "; Write(func.GetName()); os_ << "_defined_at_inversion:\n";
  os_.indent(2) << "forall (n: node) (r: reg),\n";
  os_.indent(4) << "DefinedAt "; Write(func.GetName()); os_ << " n r -> \n";

  std::vector<const Inst *> insts;
  for (const Block &block : func) {
    for (const Inst &inst : block) {
      if (inst.GetNumRets() > 0 && !inst.Is(Inst::Kind::PHI)) {
        insts.push_back(&inst);
      }
    }
  }

  for (unsigned i = 0, n = insts.size(); i < n; ++i) {
    if (i != 0) {
      os_ << "\n"; os_.indent(6) << "\\/\n";
    }
    unsigned reg = insts_[insts[i]];
    os_.indent(6);
    os_ << "(" << reg << "%positive = n /\\ " << reg << "%positive = r)";
  }
  os_ << ".\n";
  os_ << "Proof. defined_at_proof ";
  Write(func.GetName()); os_ << "_inversion ";
  Write(func.GetName()); os_ << ". Qed.\n\n";
}

// -----------------------------------------------------------------------------
void CoqEmitter::WriteUsesHaveDefs(const Func &func)
{
  os_ << "Theorem ";
  Write(func.GetName());
  os_ << "_defs_are_unique: defs_are_unique ";
  Write(func.GetName());
  os_ << ".\n";
  os_ << "Proof. defs_are_unique_proof ";
  Write(func.GetName()); os_ << "_defined_at_inversion. Qed.\n\n";
}

// -----------------------------------------------------------------------------
void CoqEmitter::WriteUsedAtInversion(const Func &func)
{
  os_ << "Theorem "; Write(func.GetName()); os_ << "_used_at_inversion:\n";
  os_.indent(2) << "forall (n: node) (r: reg),\n";
  os_.indent(4) << "UsedAt "; Write(func.GetName()); os_ << " n r -> \n";

  std::vector<std::pair<const Inst *, const Inst *>> usedAt;
  for (const Block &block : func) {
    for (const Inst &inst : block) {
      for (const Value *val : inst.operand_values()) {
        if (auto *used = ::dyn_cast_or_null<const Inst>(val)) {
          usedAt.emplace_back(&inst, used);
        }
      }
    }
  }

  for (unsigned i = 0, m = usedAt.size(); i < m; ++i) {
    if (i != 0) {
      os_ << "\n"; os_.indent(6) << "\\/\n";
    }
    auto [user, reg] = usedAt[i];
    unsigned n = insts_[user];
    unsigned r = insts_[reg];
    os_.indent(6);
    os_ << "(" << n << "%positive = n /\\ " << r << "%positive = r)";
  }
  os_ << ".\n";
  os_ << "Proof. used_at_inversion_proof ";
  Write(func.GetName()); os_ << " ";
  Write(func.GetName()); os_ << "_inversion. Qed.\n\n";
}

// -----------------------------------------------------------------------------
void CoqEmitter::WriteBlocks(const Func &func)
{
  std::vector<std::pair<const Inst *, const Block *>> insts;
  for (const Block &block : func) {
    for (const Inst &inst : block) {
      if (!inst.Is(Inst::Kind::PHI)) {
        insts.emplace_back(&inst, &block);
      }
    }
  }

  // Find paths from entry to all blocks.
  std::unordered_map<const Block *, std::vector<const Block *>> paths;
  {
    std::vector<const Block *> path;
    std::set<const Block *> visited;

    std::function<void(const Block *)> visit = [&] (const Block *block) {
      if (!visited.insert(block).second) {
        return;
      }
      paths[block] = path;
      path.push_back(block);
      for (const Block *succ : block->successors()) {
        visit(succ);
      }
      path.pop_back();
    };
    visit(&func.getEntryBlock());
  }

  // Build a reachability proof for all instructions.
  std::vector<const Inst *> nonPhis;
  {
    llvm::ReversePostOrderTraversal<const Func*> blockOrder(&func);
    std::set<const Block *> emitted;
    for (const Block *block : blockOrder) {
      for (auto it = block->begin(); it != block->end(); ++it) {
        if (it->Is(Inst::Kind::PHI)) {
          continue;
        }

        nonPhis.push_back(&*it);
        unsigned idx = insts_[&*it];
        os_ << "Theorem "; Write(func.GetName());
        os_ << "_reach_" << idx << ": Reachable "; Write(func.GetName());
        os_ << " " << idx << "%positive. Proof. ";

        if (idx == blocks_[&func.getEntryBlock()]) {
          os_ << "apply reach_entry. ";
        } else {
          unsigned idxPrev;

          const Inst *prev = nullptr;
          if (it != block->begin()) {
            auto pt = std::prev(it);
            if (!pt->Is(Inst::Kind::PHI)) {
              prev = &*pt;
            }
          }

          if (prev) {
            idxPrev = insts_[prev];
          } else {
            for (const Block *pred : block->predecessors()) {
              if (emitted.count(pred)) {
                idxPrev = insts_[pred->GetTerminator()];
                break;
              }
            }
          }

          os_ << "reach_pred_step "; Write(func.GetName());
          os_ << " " << idxPrev << "%positive. ";
          os_ << "apply "; Write(func.GetName());
          os_ << "_reach_" << idxPrev << ". ";
        }

        os_ << "Qed.\n";
      }
      emitted.insert(block);
    }
    os_ << "\n";
  }

  // Build proofs of all blocks.
  for (unsigned i = 0, n = insts.size(); i < n; ++i) {
    auto [inst, block] = insts[i];
    unsigned blockIdx = blocks_[block];
    unsigned instIdx = insts_[inst];
    os_ << "Theorem "; Write(func.GetName());
    os_ << "_bb_" << instIdx << ": BasicBlock ";
    Write(func.GetName()); os_ << " ";
    os_ << blocks_[block] << "%positive ";
    os_ << insts_[inst] << "%positive.\n";
    os_ << "Proof.\n";
    if (instIdx == blockIdx) {
      os_.indent(2) << "apply block_header.\n";
      os_.indent(2) << "apply "; Write(func.GetName());
      os_ << "_reach_" << instIdx << ".\n";
    } else {
      auto prev = insts_[&*std::prev(inst->getIterator())];
      os_.indent(2) << "block_elem_proof "; Write(func.GetName()); os_ << " ";
      Write(func.GetName()); os_ << "_inversion ";
      os_ << prev << "%positive "; Write(func.GetName());
      os_ << "_bb_" << prev << ".\n";
    }
    os_ << "Qed.\n\n";
  }
}

// -----------------------------------------------------------------------------
void CoqEmitter::WriteDominators(const Func &func)
{
  DominatorTree DT(const_cast<Func &>(func));

  std::vector<std::pair<const Block *, const Block *>> doms;

  std::function<void(const Block *)> visit = [&](const Block *b) {
    auto *node = DT.getNode(b);
    assert(node && "missing node from dominator");
    for (auto *childNode : *node) {
      auto *child = childNode->getBlock();
      doms.emplace_back(b, child);
    }
  };
  visit(&func.getEntryBlock());

  os_ << "Theorem "; Write(func.GetName()); os_ << "_bb_dom_tree:\n";

  for (unsigned i = 0, n = doms.size(); i < n; ++i) {
    if (i != 0) {
      os_ << "\n"; os_.indent(2) << "\\/\n";
    }

    auto [from, to] = doms[i];
    os_.indent(2) << "Dominates "; Write(func.GetName()); os_ << " ";
    os_ << insts_[from->GetTerminator()] << "%positive ";
    os_ << blocks_[to] << "%positive";
  }
  os_ << ".\nAdmitted.\n\n";
}

// -----------------------------------------------------------------------------
void CoqEmitter::Write(Type ty)
{
  switch (ty) {
    case Type::I8:   os_ << "I8";   return;
    case Type::I16:  os_ << "I16";  return;
    case Type::I32:  os_ << "I32";  return;
    case Type::I64:  os_ << "I64";  return;
    case Type::I128: os_ << "I128"; return;
    case Type::F32:  os_ << "F32";  return;
    case Type::F64:  os_ << "F64";  return;
    case Type::F80:  os_ << "F80";  return;
  }
  llvm_unreachable("invalid type");
}
