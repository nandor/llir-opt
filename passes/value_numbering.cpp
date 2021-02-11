// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <llvm/ADT/Statistic.h>
#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/ADT/SmallPtrSet.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/cfg.h"
#include "core/func.h"
#include "core/prog.h"
#include "core/insts.h"
#include "core/inst_compare.h"
#include "core/inst_visitor.h"
#include "core/analysis/dominator.h"
#include "passes/value_numbering.h"

#define DEBUG_TYPE "global-value-numbering"

STATISTIC(NumGlobalRenamed, "Number of global instructions renamed");
STATISTIC(NumLocalRenamed, "Number of local instructions renamed");



// -----------------------------------------------------------------------------
const char *ValueNumberingPass::kPassID = "global-value-numbering";

// -----------------------------------------------------------------------------
class ValueNumbering : InstVisitor<bool>, InstCompare {
protected:
  /// Visit each block in the dominator tree.
  unsigned Simplify(Block &block)
  {
    unsigned changed = 0;
    for (auto it = block.begin(); std::next(it) != block.end(); ) {
      Inst &inst = *it++;
      if (!Dispatch(inst)) {
        insts_[Hash(inst)].insert(&inst);
      } else {
        ++changed;
      }
    }
    return changed;
  }

  bool VisitInst(Inst &i) override { return false; }
  bool VisitConstInst(ConstInst &i) override { return Dedup(i); }
  bool VisitMovInst(MovInst &i) override { return Dedup(i); }
  bool VisitOperatorInst(OperatorInst &i) override { return Dedup(i); }

  bool Dedup(Inst &i)
  {
    for (auto *that : insts_[Hash(i)]) {
      if (IsEqual(i, *that)) {
        LLVM_DEBUG(llvm::dbgs() << i << " -> " << *that << "\n");
        i.replaceAllUsesWith(that);
        i.eraseFromParent();
        return true;
      }
    }
    return false;
  }

  bool Equal(ConstRef<Inst> a, ConstRef<Inst> b) const override
  {
    return a == b;
  }

  size_t Hash(Inst &inst)
  {
    size_t hash = static_cast<size_t>(inst.GetKind());
    for (auto value : inst.operand_values()) {
      ::hash_combine(hash, Hash(value));
    }
    return hash;
  }

  size_t Hash(Ref<Value> value)
  {
    switch (value->GetKind()) {
      case Value::Kind::CONST: {
        switch (::cast<Constant>(value)->GetKind()) {
          case Constant::Kind::INT: {
            auto ci = ::cast<ConstantInt>(value);
            return std::hash<int64_t>{}(ci->GetInt());
          }
          case Constant::Kind::FLOAT: {
            auto cf = ::cast<ConstantFloat>(value);
            return std::hash<double>{}(cf->GetDouble());
          }
        }
        llvm_unreachable("invalid constant kind");
      }
      case Value::Kind::GLOBAL: {
        size_t hash = static_cast<size_t>(Value::Kind::INST);
        ::hash_combine(hash, value.Get());
        ::hash_combine(hash, value.Index());
        return hash;
      }
      case Value::Kind::EXPR: {
        switch (::cast<Expr>(value)->GetKind()) {
          case Expr::Kind::SYMBOL_OFFSET: {
            auto expr = ::cast<SymbolOffsetExpr>(value);
            size_t hash = static_cast<size_t>(Value::Kind::INST);
            ::hash_combine(hash, expr->GetSymbol());
            ::hash_combine(hash, expr->GetOffset());
            return hash;
          }
        }
        llvm_unreachable("invalid expression kind");
      }
      case Value::Kind::INST: {
        size_t hash = static_cast<size_t>(Value::Kind::INST);
        ::hash_combine(hash, value.Get());
        ::hash_combine(hash, value.Index());
        return hash;
      }
    }
    llvm_unreachable("invalid value kind");
  }

protected:
  /// Hash table of instructions available for de-duplication.
  std::unordered_map<size_t, std::set<Inst *>> insts_;

};

// -----------------------------------------------------------------------------
class LocalValueNumbering : ValueNumbering {
public:
  LocalValueNumbering(Func &func) : func_(func){}

  bool Run()
  {
    unsigned changed = 0;
    for (Block &block : func_) {
      changed += Simplify(block);
      insts_.clear();
    }
    NumLocalRenamed += changed;
    return changed != 0;
  }

private:
  // Function to optimise.
  Func &func_;
};

// -----------------------------------------------------------------------------
class GlobalValueNumbering : ValueNumbering {
public:
  GlobalValueNumbering(Func &func) : func_(func), doms_(func_) {}

  bool Run()
  {
    unsigned changed = Visit(func_.getEntryBlock());
    NumGlobalRenamed += changed;
    return changed != 0;
  }

private:
  /// Visit each block in the dominator tree.
  unsigned Visit(Block &block)
  {
    unsigned changed = Simplify(block);

    for (const auto *child : *doms_[&block]) {
      changed += Visit(*child->getBlock());
    }

    for (Inst &inst : block) {
      insts_[Hash(inst)].erase(&inst);
    }
    return changed;
  }

private:
  /// Reference to the function.
  Func &func_;
  /// Dominator tree of the function.
  DominatorTree doms_;
};

// -----------------------------------------------------------------------------
bool ValueNumberingPass::Run(Prog &prog)
{
  bool changed = false;
  for (Func &func : prog) {
    switch (func.GetCallingConv()) {
      case CallingConv::CAML:
      case CallingConv::CAML_ALLOC:
      case CallingConv::CAML_GC: {
        changed = LocalValueNumbering(func).Run() || changed;
        continue;
      }
      case CallingConv::C:
      case CallingConv::SETJMP:
      case CallingConv::XEN:
      case CallingConv::INTR: {
        changed = GlobalValueNumbering(func).Run() || changed;
        continue;
      }
    }
    llvm_unreachable("invalid calling convention");
  }
  return changed;
}

// -----------------------------------------------------------------------------
const char *ValueNumberingPass::GetPassName() const
{
  return "Move Elimination";
}
