// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/SmallPtrSet.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/func.h"
#include "core/prog.h"
#include "core/insts.h"
#include "passes/dedup_block.h"


// -----------------------------------------------------------------------------
const char *DedupBlockPass::kPassID = "dedup-block";

// -----------------------------------------------------------------------------
void DedupBlockPass::Run(Prog *prog)
{
  for (Func &func : *prog) {
    DedupExits(&func);
  }
}

// -----------------------------------------------------------------------------
void DedupBlockPass::DedupExits(Func *func)
{
  for (auto it = func->begin(); it != func->end(); ) {
    Block *b1 = &*it++;
    for (auto jt = it; jt != func->end(); ) {
      Block *b2 = &*jt++;
      if (IsDuplicateExit(b1, b2)) {
        b2->replaceAllUsesWith(b1);
        if (&*std::next(b1->getIterator()) == b2) {
          ++it;
        }
        b2->eraseFromParent();
      }
    }
  }
}

// -----------------------------------------------------------------------------
bool DedupBlockPass::IsDuplicateExit(const Block *b1, const Block *b2)
{
  if (b1->size() != b2->size())
    return false;
  if (!b1->succ_empty() || !b2->succ_empty())
    return false;
  if (b1->begin()->Is(Inst::Kind::PHI) || b2->begin()->Is(Inst::Kind::PHI))
    return false;

  auto itb1 = b1->begin();
  auto itb2 = b2->begin();
  InstMap insts;
  while (itb1 != b1->end() && itb2 != b2->end()) {
    if (!IsEqual(&*itb1, &*itb2, insts)) {
      return false;
    }
    insts.insert({ &*itb1, &*itb2 });
    ++itb1;
    ++itb2;
  }
  return itb1 == b1->end() && itb2 == b2->end();
}

// -----------------------------------------------------------------------------
bool DedupBlockPass::IsEqual(const Inst *i1, const Inst *i2, InstMap &insts)
{
  if (i1->GetKind() != i2->GetKind())
    return false;
  if (i1->size() != i2->size())
    return false;
  if (i1->GetAnnots() != i2->GetAnnots())
    return false;

  switch (i1->GetKind()) {
    case Inst::Kind::CALL: {
      auto *call1 = static_cast<const CallInst *>(i1);
      auto *call2 = static_cast<const CallInst *>(i2);
      if (call1->GetCallingConv() != call2->GetCallingConv())
        return false;
      break;
    }
    case Inst::Kind::TCALL:
    case Inst::Kind::INVOKE: {
      auto *call1 = static_cast<const CallSite<TerminatorInst> *>(i1);
      auto *call2 = static_cast<const CallSite<TerminatorInst> *>(i2);
      if (call1->GetCallingConv() != call2->GetCallingConv())
        return false;
      break;
    }
    case Inst::Kind::CMP: {
      auto *cmp1 = static_cast<const CmpInst *>(i1);
      auto *cmp2 = static_cast<const CmpInst *>(i2);
      if (cmp1->GetCC() != cmp2->GetCC())
        return false;
      break;
    }
    default: {
      break;
    }
  }

  auto vt1 = i1->value_op_begin();
  auto vt2 = i2->value_op_begin();
  while (vt1 != i1->value_op_end() && vt2 != i2->value_op_end()) {
    if (vt1->GetKind() != vt2->GetKind())
      return false;

    switch (vt1->GetKind()) {
      case Value::Kind::INST: {
        auto *it1 = static_cast<const Inst *>(*vt1);
        auto *it2 = static_cast<const Inst *>(*vt2);
        if (it1 != it2) {
          auto it = insts.find(it1);
          if (it == insts.end() || it->second != it2) {
            return false;
          }
        }
        break;
      }
      case Value::Kind::GLOBAL: {
        if (*vt1 != *vt2) {
          return false;
        }
        break;
      }
      case Value::Kind::EXPR: {
        switch (static_cast<const Expr *>(*vt1)->GetKind()) {
          case Expr::Kind::SYMBOL_OFFSET: {
            auto *e1 = static_cast<const SymbolOffsetExpr *>(*vt1);
            auto *e2 = static_cast<const SymbolOffsetExpr *>(*vt2);
            if (e1->GetSymbol() != e2->GetSymbol())
              return false;
            if (e1->GetOffset() != e2->GetOffset())
              return false;
            break;
          }
        }
        break;
      }
      case Value::Kind::CONST: {
        switch (static_cast<const Constant *>(*vt1)->GetKind()) {
          case Constant::Kind::INT: {
            auto *v1 = static_cast<const ConstantInt *>(*vt1);
            auto *v2 = static_cast<const ConstantInt *>(*vt2);
            const auto &int1 = v1->GetValue();
            const auto &int2 = v2->GetValue();
            if (int1.getBitWidth() != int2.getBitWidth()) {
              return false;
            }
            if (int1 != int2) {
              return false;
            }
            break;
          }
          case Constant::Kind::FLOAT: {
            auto *v1 = static_cast<const ConstantFloat *>(*vt1);
            auto *v2 = static_cast<const ConstantFloat *>(*vt2);
            const auto &float1 = v1->GetValue();
            const auto &float2 = v2->GetValue();
            if (float1.bitwiseIsEqual(float2) != llvm::APFloat::cmpEqual) {
              return false;
            }
            break;
          }
          case Constant::Kind::REG: {
            auto *v1 = static_cast<const ConstantReg *>(*vt1);
            auto *v2 = static_cast<const ConstantReg *>(*vt2);
            if (v1->GetValue() != v2->GetValue()) {
              return false;
            }
            break;
          }
        }
        break;
      }
    }
    ++vt1;
    ++vt2;
  }
  return vt1 == i1->value_op_end() && vt2 == i2->value_op_end();
}

// -----------------------------------------------------------------------------
const char *DedupBlockPass::GetPassName() const
{
  return "Block Deduplication";
}
