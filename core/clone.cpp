// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/PostOrderIterator.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/cfg.h"
#include "core/clone.h"
#include "core/prog.h"



// -----------------------------------------------------------------------------
CloneVisitor::~CloneVisitor()
{
}

// -----------------------------------------------------------------------------
void CloneVisitor::Fixup()
{
  for (auto &phi : fixups_) {
    auto *phiOld = phi.first;
    auto *phiNew = phi.second;
    for (unsigned i = 0; i < phiOld->GetNumIncoming(); ++i) {
      phiNew->Add(Map(phiOld->GetBlock(i)), Map(phiOld->GetValue(i)));
    }
  }
  fixups_.clear();
}

// -----------------------------------------------------------------------------
Ref<Value> CloneVisitor::Map(Ref<Value> value)
{
  switch (value->GetKind()) {
    case Value::Kind::INST: return Map(cast<Inst>(value));
    case Value::Kind::GLOBAL: return Map(&*cast<Global>(value));
    case Value::Kind::EXPR: return Map(&*cast<Expr>(value));
    case Value::Kind::CONST: return Map(&*cast<Constant>(value));
  }
  llvm_unreachable("invalid value kind");
}

// -----------------------------------------------------------------------------
Global *CloneVisitor::Map(Global *global)
{
  switch (global->GetKind()) {
    case Global::Kind::EXTERN: return Map(static_cast<Extern *>(global));
    case Global::Kind::FUNC:   return Map(static_cast<Func *>(global));
    case Global::Kind::BLOCK:  return Map(static_cast<Block *>(global));
    case Global::Kind::ATOM:   return Map(static_cast<Atom *>(global));
  }
  llvm_unreachable("invalid global kind");
}

// -----------------------------------------------------------------------------
Expr *CloneVisitor::Map(Expr *expr)
{
  switch (expr->GetKind()) {
    case Expr::Kind::SYMBOL_OFFSET: {
      auto *symOff = static_cast<SymbolOffsetExpr *>(expr);
      if (auto *sym = symOff->GetSymbol()) {
        return SymbolOffsetExpr::Create(Map(sym), symOff->GetOffset());
      } else {
        return SymbolOffsetExpr::Create(nullptr, symOff->GetOffset());
      }
    }
  }
  llvm_unreachable("invalid expression kind");
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(Inst *i)
{
  switch (i->GetKind()) {
    #define GET_INST(kind, type, name, sort) \
      case Inst::Kind::kind: return Clone(static_cast<type *>(i));
    #include "instructions.def"
  }
  llvm_unreachable("invalid instruction kind");
}

// -----------------------------------------------------------------------------
AnnotSet CloneVisitor::Annot(const Inst *inst)
{
  return inst->GetAnnots();
}

// -----------------------------------------------------------------------------
Inst *CloneVisitor::Clone(PhiInst *i)
{
  auto *phi = new PhiInst(Map(i->GetType(), i, 0), Annot(i));
  fixups_.emplace_back(i, phi);
  return phi;
}

// -----------------------------------------------------------------------------
class ProgramCloneVisitor : public CloneVisitor {
public:
  ~ProgramCloneVisitor();

  std::pair<std::unique_ptr<Prog>, Inst *> Clone(Prog *oldProg, Inst *inst);

  Ref<Inst> Map(Ref<Inst> inst) override
  {
    if (auto it = insts_.find(inst); it != insts_.end()) {
      return it->second;
    }
    llvm_unreachable("instruction not duplicated");
  }

  Block *Map(Block *oldBlock) override
  {
    auto it = globals_.emplace(oldBlock, nullptr);
    if (it.second) {
      auto *newBlock = new Block(oldBlock->GetName());
      it.first->second = newBlock;
      return newBlock;
    } else {
      return ::cast<Block>(it.first->second);
    }
  }

  Func *Map(Func *oldFunc) override
  {
    auto it = globals_.emplace(oldFunc, nullptr);
    if (it.second) {
      auto *newFunc = new Func(
          oldFunc->GetName(),
          oldFunc->GetVisibility()
      );
      it.first->second = newFunc;
      return newFunc;
    } else {
      return ::cast<Func>(it.first->second);
    }
  }

  Extern *Map(Extern *oldExt) override
  {
    auto it = globals_.emplace(oldExt, nullptr);
    if (it.second) {
      auto *newExt = new Extern(
          oldExt->GetName(),
          oldExt->GetVisibility()
      );
      it.first->second = newExt;
      return newExt;
    } else {
      return ::cast<Extern>(it.first->second);
    }
  }

  Atom *Map(Atom *oldAtom) override
  {
    auto it = globals_.emplace(oldAtom, nullptr);
    if (it.second) {
      auto *newAtom = new Atom(
          oldAtom->GetName(),
          oldAtom->GetVisibility(),
          oldAtom->GetAlignment()
      );
      it.first->second = newAtom;
      return newAtom;
    } else {
      return ::cast<Atom>(it.first->second);
    }
  }

  Constant *Map(Constant *oldConst) override
  {
    switch (oldConst->GetKind()) {
      case Constant::Kind::INT: {
        return new ConstantInt(static_cast<ConstantInt *>(oldConst)->GetValue());
      }
      case Constant::Kind::FLOAT: {
        return new ConstantFloat(static_cast<ConstantFloat *>(oldConst)->GetValue());
      }
    }
    llvm_unreachable("invalid constant kind");
  }

private:
  std::unordered_map<Global *, Global *> globals_;
  std::unordered_map<Ref<Inst>, Ref<Inst>> insts_;
};

// -----------------------------------------------------------------------------
ProgramCloneVisitor::~ProgramCloneVisitor()
{
}

// -----------------------------------------------------------------------------
std::pair<std::unique_ptr<Prog>, Inst *>
ProgramCloneVisitor::Clone(Prog *oldProg, Inst *inst)
{
  auto newProg = std::make_unique<Prog>(oldProg->GetName());

  for (Extern &oldExt : oldProg->externs()) {
    newProg->AddExtern(Map(&oldExt));
  }

  for (Data &oldData : oldProg->data()) {
    Data *newData = new Data(oldData.GetName());
    newProg->AddData(newData);
    for (Object &oldObject : oldData) {
      Object *newObject = new Object();
      newData->AddObject(newObject);
      for (Atom &oldAtom : oldObject) {
        Atom *newAtom = Map(&oldAtom);
        newObject->AddAtom(newAtom);
        for (Item &oldItem : oldAtom) {
          switch (oldItem.GetKind()) {
            case Item::Kind::INT8: {
              newAtom->AddItem(Item::CreateInt8(oldItem.GetInt8()));
              break;
            }
            case Item::Kind::INT16: {
              newAtom->AddItem(Item::CreateInt16(oldItem.GetInt16()));
              break;
            }
            case Item::Kind::INT32: {
              newAtom->AddItem(Item::CreateInt32(oldItem.GetInt32()));
              break;
            }
            case Item::Kind::INT64: {
              newAtom->AddItem(Item::CreateInt64(oldItem.GetInt64()));
              break;
            }
            case Item::Kind::FLOAT64: {
              newAtom->AddItem(Item::CreateFloat64(oldItem.GetFloat64()));
              break;
            }
            case Item::Kind::EXPR32: {
              newAtom->AddItem(Item::CreateExpr32(
                  CloneVisitor::Map(oldItem.GetExpr())
              ));
              break;
            }
            case Item::Kind::EXPR64: {
              newAtom->AddItem(Item::CreateExpr64(
                  CloneVisitor::Map(oldItem.GetExpr())
              ));
              break;
            }
            case Item::Kind::SPACE: {
              newAtom->AddItem(Item::CreateSpace(oldItem.GetSpace()));
              break;
            }
            case Item::Kind::STRING: {
              newAtom->AddItem(Item::CreateString(oldItem.GetString()));
              break;
            }
          }
        }
      }
    }
  }
  Inst *mappedInst = nullptr;
  for (Func &oldFunc : oldProg->funcs()) {
    Func *newFunc = Map(&oldFunc);
    newFunc->SetCallingConv(oldFunc.GetCallingConv());
    newFunc->SetParameters(oldFunc.params());
    newFunc->SetVarArg(oldFunc.IsVarArg());
    newFunc->SetNoInline(oldFunc.IsNoInline());
    if (auto align = oldFunc.GetAlignment()) {
      newFunc->SetAlignment(*align);
    }
    for (auto &obj : oldFunc.objects()) {
      newFunc->AddStackObject(obj.Index, obj.Size, obj.Alignment);
    }
    llvm::ReversePostOrderTraversal<Func*> rpot(&oldFunc);
    for (auto *oldBlock : rpot) {
      Block *newBlock = Map(oldBlock);
      newFunc->AddBlock(newBlock);
      for (auto &oldInst : *oldBlock) {
        auto *newInst = CloneVisitor::Clone(&oldInst);
        if (&oldInst == inst) {
          mappedInst = newInst;
        }
        assert(oldInst.GetNumRets() == newInst->GetNumRets() && "bad clone");
        for (unsigned i = 0, n = oldInst.GetNumRets(); i < n; ++i) {
          insts_.emplace(oldInst.GetSubValue(i), newInst->GetSubValue(i));
        }
        newBlock->AddInst(newInst);
      }
    }
    newProg->AddFunc(newFunc);
    CloneVisitor::Fixup();
    insts_.clear();
  }

  return { std::move(newProg), mappedInst };
}

// -----------------------------------------------------------------------------
std::pair<std::unique_ptr<Prog>, Inst *> Clone(Prog &oldProg, Inst *inst)
{
  return ProgramCloneVisitor().Clone(&oldProg, inst);
}

// -----------------------------------------------------------------------------
std::unique_ptr<Prog> Clone(Prog &oldProg)
{
  return std::move(ProgramCloneVisitor().Clone(&oldProg, nullptr).first);
}

#define GET_CLONE_IMPL
#include "instructions.def"
