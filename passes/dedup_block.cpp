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
  if (i1->GetKind() != i2->GetKind()) {
    return false;
  }
  if (i1->size() != i2->size()) {
    return false;
  }
  if (i1->GetAnnots() != i2->GetAnnots()) {
    return false;
  }

  // Check additional attributes.
  switch (i1->GetKind()) {
    case Inst::Kind::CALL:
    case Inst::Kind::INVOKE:
    case Inst::Kind::TCALL: {
      auto *call1 = static_cast<const CallSite *>(i1);
      auto *call2 = static_cast<const CallSite *>(i2);
      if (call1->GetCallingConv() != call2->GetCallingConv()) {
        return false;
      }
      if (call1->type_size() != call2->type_size()) {
        return false;
      }
      for (unsigned i = 0, n = call1->type_size(); i < n; ++i) {
        if (call1->type(i) != call2->type(i)) {
          return false;
        }
      }
      break;
    }
    case Inst::Kind::CMP: {
      auto *cmp1 = static_cast<const CmpInst *>(i1);
      auto *cmp2 = static_cast<const CmpInst *>(i2);
      if (cmp1->GetCC() != cmp2->GetCC()) {
        return false;
      }
      break;
    }
    default: {
      break;
    }
  }

  // Check the return type.
  if (i1->GetNumRets() != i2->GetNumRets()) {
    return false;
  }
  for (unsigned i = 0, n = i1->GetNumRets(); i < n; ++i) {
    if (i1->GetType(i) != i2->GetType(i)) {
      return false;
    }
  }

  // Check individual arguments.
  auto vt1 = i1->value_op_begin();
  auto vt2 = i2->value_op_begin();
  while (vt1 != i1->value_op_end() && vt2 != i2->value_op_end()) {
    if ((*vt1).Index() != (*vt2).Index()) {
      return false;
    }
    if (vt1->GetKind() != vt2->GetKind()) {
      return false;
    }

    switch (vt1->GetKind()) {
      case Value::Kind::INST: {
        auto it1 = cast<Inst>(*vt1);
        auto it2 = cast<Inst>(*vt2);
        if (it1 != it2) {
          auto it = insts.find(it1.Get());
          if (it == insts.end() || it->second != it2.Get()) {
            return false;
          }
        }
        break;
      }
      case Value::Kind::GLOBAL: {
        if ((*vt1).Get() != (*vt2).Get()) {
          return false;
        };
        break;
      }
      case Value::Kind::EXPR: {
        auto &et1 = *cast<Expr>(*vt1);
        auto &et2 = *cast<Expr>(*vt2);
        if (et1.GetKind() != et2.GetKind()) {
          return false;
        }
        switch (et1.GetKind()) {
          case Expr::Kind::SYMBOL_OFFSET: {
            auto &e1 = static_cast<const SymbolOffsetExpr &>(et1);
            auto &e2 = static_cast<const SymbolOffsetExpr &>(et2);
            if (e1.GetSymbol() != e2.GetSymbol())
              return false;
            if (e1.GetOffset() != e2.GetOffset())
              return false;
            break;
          }
        }
        break;
      }
      case Value::Kind::CONST: {
        auto &ct1 = *cast<Constant>(*vt1);
        auto &ct2 = *cast<Constant>(*vt2);
        if (ct1.GetKind() != ct2.GetKind()) {
          return false;
        }
        switch (ct1.GetKind()) {
          case Constant::Kind::INT: {
            auto &v1 = static_cast<const ConstantInt &>(ct1);
            auto &v2 = static_cast<const ConstantInt &>(ct2);
            const auto &int1 = v1.GetValue();
            const auto &int2 = v2.GetValue();
            if (int1.getBitWidth() != int2.getBitWidth()) {
              return false;
            }
            if (int1 != int2) {
              return false;
            }
            break;
          }
          case Constant::Kind::FLOAT: {
            auto &v1 = static_cast<const ConstantFloat &>(ct1);
            auto &v2 = static_cast<const ConstantFloat &>(ct2);
            const auto &float1 = v1.GetValue();
            const auto &float2 = v2.GetValue();
            if (float1.bitwiseIsEqual(float2) != llvm::APFloat::cmpEqual) {
              return false;
            }
            break;
          }
          case Constant::Kind::REG: {
            auto &v1 = static_cast<const ConstantReg &>(ct1);
            auto &v2 = static_cast<const ConstantReg &>(ct2);
            if (v1.GetValue() != v2.GetValue()) {
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
