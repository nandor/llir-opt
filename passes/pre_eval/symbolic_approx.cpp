// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <queue>

#include "core/extern.h"
#include "core/atom.h"

#include <llvm/Support/Debug.h>

#include "core/analysis/reference_graph.h"
#include "passes/pre_eval/pointer_closure.h"
#include "passes/pre_eval/symbolic_approx.h"
#include "passes/pre_eval/symbolic_context.h"
#include "passes/pre_eval/symbolic_eval.h"
#include "passes/pre_eval/symbolic_heap.h"
#include "passes/pre_eval/symbolic_value.h"

#define DEBUG_TYPE "pre-eval"



// -----------------------------------------------------------------------------
bool IsAllocation(Func &func)
{
  auto name = func.getName();
  return name == "malloc"
      || name == "free"
      || name == "realloc"
      || name == "caml_alloc_shr"
      || name == "caml_alloc_shr_aux"
      || name == "caml_alloc_small_aux"
      || name == "caml_alloc1"
      || name == "caml_alloc2"
      || name == "caml_alloc3"
      || name == "caml_allocN"
      || name == "caml_alloc_custom_mem"
      || name == "caml_gc_dispatch";
}

// -----------------------------------------------------------------------------
bool SymbolicApprox::Approximate(CallSite &call)
{
  auto &frame = *ctx_.GetActiveFrame();
  auto index = frame.GetIndex();
  if (auto *func = call.GetDirectCallee()) {
    if (IsAllocation(*func)) {
      LLVM_DEBUG(llvm::dbgs() << "Allocation " << func->getName() << "\n");
      if (func->getName() == "malloc") {
        if (call.arg_size() == 1 && call.type_size() == 1) {
          return Malloc(call, frame.Find(call.arg(0)).AsInt());
        } else {
          llvm_unreachable("not implemented");
        }
      } else if (func->getName() == "free") {
        // TODO: invalidate the object?
        return false;
      } else if (func->getName() == "realloc") {
        if (call.arg_size() == 2 && call.type_size() == 1) {
          return Realloc(
              call,
              frame.Find(call.arg(0)),
              frame.Find(call.arg(1)).AsInt()
          );
        } else {
          llvm_unreachable("not implemented");
        }
      } else if (func->getName() == "caml_alloc_small_aux" || func->getName() == "caml_alloc_shr_aux") {
        if (call.arg_size() >= 1 && call.type_size() == 1) {
          auto orig = std::make_pair(index, call.GetSubValue(0));
          if (auto size = frame.Find(call.arg(0)).AsInt()) {
            auto ptr = ctx_.Malloc(call, size->getZExtValue() * 8);
            LLVM_DEBUG(llvm::dbgs() << "\t\t0: " << *ptr << "\n");
            return frame.Set(call, SymbolicValue::Nullable(ptr, orig));
          } else {
            auto ptr = ctx_.Malloc(call, std::nullopt);
            LLVM_DEBUG(llvm::dbgs() << "\t\t0: " << *ptr << "\n");
            return frame.Set(call, SymbolicValue::Nullable(ptr, orig));
          }
        } else {
          llvm_unreachable("not implemented");
        }
      } else if (func->getName() == "caml_alloc1") {
        if (call.arg_size() == 2 && call.type_size() == 2) {
          auto orig = std::make_pair(index, call.GetSubValue(1));
          auto ptr = SymbolicValue::Nullable(ctx_.Malloc(call, 16), orig);
          bool changed = false;
          frame.Set(call.GetSubValue(0), frame.Find(call.arg(0))) || changed;
          frame.Set(call.GetSubValue(1), ptr) || changed;
          return changed;
        } else {
          llvm_unreachable("not implemented");
        }
      } else if (func->getName() == "caml_alloc2") {
        if (call.arg_size() == 2 && call.type_size() == 2) {
          auto orig = std::make_pair(index, call.GetSubValue(1));
          auto ptr = SymbolicValue::Nullable(ctx_.Malloc(call, 24), orig);
          bool changed = false;
          frame.Set(call.GetSubValue(0), frame.Find(call.arg(0))) || changed;
          frame.Set(call.GetSubValue(1), ptr) || changed;
          return changed;
        } else {
          llvm_unreachable("not implemented");
        }
      } else if (func->getName() == "caml_alloc3") {
        if (call.arg_size() == 2 && call.type_size() == 2) {
          auto orig = std::make_pair(index, call.GetSubValue(1));
          auto ptr = SymbolicValue::Nullable(ctx_.Malloc(call, 32), orig);
          bool changed = false;
          frame.Set(call.GetSubValue(0), frame.Find(call.arg(0))) || changed;
          frame.Set(call.GetSubValue(1), ptr) || changed;
          return changed;
        } else {
          llvm_unreachable("not implemented");
        }
      } else if (func->getName() == "caml_allocN") {
        if (call.arg_size() == 2 && call.type_size() == 2) {
          auto orig = std::make_pair(index, call.GetSubValue(1));
          if (auto sub = ::cast_or_null<SubInst>(call.arg(1))) {
            if (auto mov = ::cast_or_null<MovInst>(sub->GetRHS())) {
              if (auto val = ::cast_or_null<ConstantInt>(mov->GetArg())) {
                auto ptr = SymbolicValue::Nullable(
                    ctx_.Malloc(call, val->GetInt()),
                    orig
                );
                bool changed = false;
                frame.Set(call.GetSubValue(0), frame.Find(call.arg(0))) || changed;
                frame.Set(call.GetSubValue(1), ptr) || changed;
                return changed;
              } else {
                llvm_unreachable("not implemented");
              }
            } else {
              llvm_unreachable("not implemented");
            }
          } else {
            llvm_unreachable("not implemented");
          }
        } else {
          llvm_unreachable("not implemented");
        }
      } else if (func->getName() == "caml_alloc_custom_mem") {
        if (call.arg_size() == 3 && call.type_size() == 1) {
          if (auto size = frame.Find(call.arg(1)).AsInt()) {
            auto ptr = ctx_.Malloc(call, size->getZExtValue());
            LLVM_DEBUG(llvm::dbgs() << "\t\t0: " << *ptr << "\n");
            return frame.Set(call, SymbolicValue::Nullable(ptr));
          } else {
            llvm_unreachable("not implemented");
          }
        } else {
          llvm_unreachable("not implemented");
        }
      } else if (func->getName() == "caml_gc_dispatch") {
        return false;
      } else {
        llvm_unreachable("not implemented");
      }
    } else {
      return ApproximateCall(call);
    }
  } else {
    return ApproximateCall(call);
  }
}

// -----------------------------------------------------------------------------
void SymbolicApprox::Approximate(
    SymbolicFrame &frame,
    const std::set<SCCNode *> &bypassed,
    const std::set<SymbolicContext *> &contexts)
{
  // Compute the union of all contexts.
  LLVM_DEBUG(llvm::dbgs() << "Merging " << contexts.size() << " contexts\n");
  for (auto &context : contexts) {
    ctx_.Merge(*context);
  }

  // If any nodes were bypassed, collect all references inside those
  // nodes, along with all additional symbols introduced in the branch.
  // Compute the transitive closure of these objects, tainting all
  // pointees with the closure as a pointer in the unified heap
  // before merging it into the current state. Map all values to this
  // tainted value, with the exception of obvious trivial constants.
  LLVM_DEBUG(llvm::dbgs() << "Collecting references\n");
  SymbolicPointer::Ref uses;
  std::set<CallSite *> calls;
  std::set<CallSite *> allocs;

  auto addOperand = [&](Ref<Value> opValue)
  {
    Ref<Inst> opInst = ::cast_or_null<Inst>(opValue);
    if (!opInst) {
      return;
    }
    auto *usedValue = frame.FindOpt(*opInst);
    if (!usedValue) {
      return;
    }
    if (auto ptr = usedValue->AsPointer()) {
      LLVM_DEBUG(llvm::dbgs() << "\t\t" << *ptr << "\n");
      if (uses) {
        uses->Merge(ptr->Decay());
      } else {
        uses = ptr->Decay();
      }
    }
  };

  for (auto *node : bypassed) {
    for (Block *block : node->Blocks) {
      for (Inst &inst : *block) {
        LLVM_DEBUG(llvm::dbgs() << "\tScan " << inst << "\n");
        if (auto *call = ::cast_or_null<CallSite>(&inst)) {
          if (auto *f = call->GetDirectCallee()) {
            auto n = f->getName();
            if (IsAllocation(*f)) {
              allocs.insert(call);
            } else {
              calls.insert(call);
            }
          } else {
            calls.insert(call);
          }
          for (auto op : call->args()) {
            addOperand(op);
          }
        } else {
          for (Ref<Value> op : inst.operand_values()) {
            addOperand(op);
          }
        }
      }
    }
  }

  auto value = uses ? SymbolicValue::Value(uses) : SymbolicValue::Scalar();
  auto approx = ApproximateNodes(calls, allocs, value, ctx_);

  // Set the values defined in the blocks.
  for (auto *node : bypassed) {
    for (Block *block : node->Blocks) {
      frame.Approximate(block);
      LLVM_DEBUG(llvm::dbgs() << "\tBypass: " << block->getName() << '\n');
      for (Inst &inst : *block) {
        LLVM_DEBUG(llvm::dbgs() << "\tApprox: " << inst << '\n');
        if (auto *mov = ::cast_or_null<MovInst>(&inst)) {
          Resolve(frame, *mov, value);
          continue;
        }
        if (auto *load = ::cast_or_null<MemoryLoadInst>(&inst)) {
          frame.Set(load, approx.Taint);
          continue;
        }
        if (auto *store = ::cast_or_null<MemoryStoreInst>(&inst)) {
          ctx_.Taint(approx.Taint, approx.Tainted);
          continue;
        }
        if (auto *xchg = ::cast_or_null<MemoryExchangeInst>(&inst)) {
          llvm_unreachable("not implemented");
        }
        for (unsigned i = 0, n = inst.GetNumRets(); i < n; ++i) {
          frame.Set(inst.GetSubValue(i), approx.Taint);
        }
      }
    }
  }

  // Raise, if necessary.
  if (approx.Raises) {
    Raise(ctx_, value);
  }
}

// -----------------------------------------------------------------------------
bool SymbolicApprox::ApproximateCall(CallSite &call)
{
  SymbolicValue value = SymbolicValue::Scalar();
  auto &frame = *ctx_.GetActiveFrame();
  for (auto arg : call.args()) {
    auto argVal = frame.Find(arg);
    LLVM_DEBUG(llvm::dbgs() << "\t\t\t" << argVal << "\n");
    value = value.LUB(argVal);
  }
  auto approx = ApproximateNodes({ &call }, {}, value, ctx_);
  bool changed = approx.Changed;
  for (unsigned i = 0, n = call.GetNumRets(); i < n; ++i) {
    changed = frame.Set(call.GetSubValue(i), approx.Taint) || changed;
  }
  // Raise, if necessary.
  if (approx.Raises) {
    changed = Raise(ctx_, value) || changed;
  }
  return changed;
}

// -----------------------------------------------------------------------------
SymbolicApprox::Approximation SymbolicApprox::ApproximateNodes(
    const std::set<CallSite *> &calls,
    const std::set<CallSite *> &allocs,
    SymbolicValue &refs,
    SymbolicContext &ctx)
{
  PointerClosure closure(heap_, ctx);
  bool indirect = false;
  bool raises = false;

  // Find items referenced from the values.
  closure.Add(refs);

  // Find items referenced by the calls.
  for (auto *call : calls) {
    if (auto *f = call->GetDirectCallee()) {
      LLVM_DEBUG(llvm::dbgs() << "Direct call: " << f->getName() << "\n");
      auto &node = refs_[*f];
      indirect = indirect || node.HasIndirectCalls;
      raises = raises || node.HasRaise;
      for (auto *g : node.Escapes) {
        LLVM_DEBUG(llvm::dbgs() << "\t" << g->getName() << "\n");
        switch (g->GetKind()) {
          case Global::Kind::FUNC: {
            closure.Add(static_cast<Func *>(g));
            continue;
          }
          case Global::Kind::ATOM: {
            auto *obj = static_cast<Atom *>(g)->getParent();
            closure.AddEscaped(obj);
            continue;
          }
          case Global::Kind::EXTERN: {
            // TODO: follow externs.
            continue;
          }
          case Global::Kind::BLOCK: {
            // TODO: add blocks.
            continue;
          }
        }
        llvm_unreachable("invalid global kind");
      }
      for (auto *o : node.Read) {
        closure.AddRead(o);
      }
      for (auto *o : node.Written) {
        closure.AddWritten(o);
      }
    } else {
      indirect = true;
    }
  }

  // If there are indirect calls, iterate until convergence.
  if (indirect) {
    BitSet<Func> visited;
    std::queue<ID<Func>> qf;
    for (auto id : closure.funcs()) {
      qf.push(id);
    }
    while (!qf.empty()) {
      auto id = qf.front();
      qf.pop();
      if (!visited.Insert(id)) {
        continue;
      }

      auto &node = refs_[heap_.Map(id)];
      raises = raises || node.HasRaise;
      for (auto *g : node.Escapes) {
        switch (g->GetKind()) {
          case Global::Kind::FUNC: {
            qf.push(heap_.Function(static_cast<Func *>(g)));
            continue;
          }
          case Global::Kind::ATOM: {
            auto *obj = static_cast<Atom *>(g)->getParent();
            closure.AddEscaped(obj);
            continue;
          }
          case Global::Kind::EXTERN: {
            // TODO: follow externs.
            continue;
          }
          case Global::Kind::BLOCK: {
            // TODO: add blocks.
            continue;
          }
        }
        llvm_unreachable("invalid global kind");
      }
      for (auto *o : node.Read) {
        closure.AddRead(o);
      }
      for (auto *o : node.Written) {
        closure.AddWritten(o);
      }
      for (auto id : closure.funcs()) {
        if (!visited.Contains(id)) {
          qf.push(id);
        }
      }
    }
  }

  // Apply the effect of the transitive closure.
  auto pTainted = closure.BuildTainted();
  auto tainted = pTainted ? SymbolicValue::Value(pTainted) : SymbolicValue::Scalar();
  auto pTaint = closure.BuildTaint();
  auto taint = pTaint ? SymbolicValue::Value(pTaint) : SymbolicValue::Scalar();

  bool changed;
  if (pTainted) {
    LLVM_DEBUG(llvm::dbgs() << "Tainting " << tainted << " with " << taint << "\n");
    changed = ctx.Store(*pTainted, taint, Type::I64);
  } else {
    changed = false;
  }

  return { changed, raises, taint, tainted };
}

// -----------------------------------------------------------------------------
bool SymbolicApprox::Raise(
    SymbolicContext &ctx,
    const SymbolicValue &taint)
{
  // Taint all landing pads on the stack which can be reached from here.
  // Landing pads must be tainted with incoming values in case the
  // evaluation of an invoke instruction continues with the catch block.
  bool changed = false;
  if (auto ptr = taint.AsPointer()) {
    std::set<Block *> blocks;
    for (auto *block : ptr->blocks()) {
      blocks.insert(block);
    }
    for (auto &frame : ctx.frames()) {
      // See whether the block is among the successors of the active node
      // in any of the frames on the stack, propagating to landing pads.
      auto *exec = frame.GetCurrentBlock();
      if (!exec) {
        continue;
      }
      for (auto *block : exec->successors()) {
        if (!blocks.count(block)) {
          continue;
        }
        LLVM_DEBUG(llvm::dbgs()
            << "\t\tLanding: " << block->getName() << " " << &ctx << "\n"
        );
        for (auto &inst : *block) {
          auto *pad = ::cast_or_null<LandingPadInst>(&inst);
          if (!pad) {
            continue;
          }
          LLVM_DEBUG(llvm::dbgs() << "\t\t\t" << inst << "\n");
          for (unsigned i = 0, n = pad->GetNumRets(); i < n; ++i) {
            changed = frame.Set(pad->GetSubValue(i), taint) || changed;
          }
        }
        frame.Bypass(frame.GetNode(block), ctx);
      }
    }
  }
  return changed;
}

// -----------------------------------------------------------------------------
void SymbolicApprox::Resolve(
    SymbolicFrame &frame,
    MovInst &mov,
    const SymbolicValue &taint)
{
  // Try to register constants introduced by mov as constants
  // instead of relying on the universal over-approximated value.
  auto arg = mov.GetArg();
  switch (arg->GetKind()) {
    case Value::Kind::INST: {
      frame.Set(mov, taint);
      return;
    }
    case Value::Kind::GLOBAL: {
      switch (::cast<Global>(arg)->GetKind()) {
        case Global::Kind::ATOM: {
          frame.Set(mov, SymbolicValue::Pointer(
              ctx_.Pointer(*::cast<Atom>(arg), 0)
          ));
          return;
        }
        case Global::Kind::EXTERN: {
          frame.Set(mov, SymbolicValue::Pointer(
              std::make_shared<SymbolicPointer>(&*::cast<Extern>(arg), 0))
          );
          return;
        }
        case Global::Kind::FUNC: {
          frame.Set(mov, SymbolicValue::Pointer(
              std::make_shared<SymbolicPointer>(
                  heap_.Function(&*::cast<Func>(arg))
              )
          ));
          return;
        }
        case Global::Kind::BLOCK: {
          frame.Set(mov, SymbolicValue::Pointer(
              std::make_shared<SymbolicPointer>(&*::cast<Block>(arg)))
          );
          return;
        }
      }
      llvm_unreachable("invalid global kind");
    }
    case Value::Kind::EXPR: {
      switch (::cast<Expr>(arg)->GetKind()) {
        case Expr::Kind::SYMBOL_OFFSET: {
          auto se = ::cast<SymbolOffsetExpr>(arg);
          auto sym = se->GetSymbol();
          auto off = se->GetOffset();
          switch (sym->GetKind()) {
            case Global::Kind::ATOM: {
              frame.Set(mov, SymbolicValue::Pointer(
                  ctx_.Pointer(*::cast<Atom>(sym), off)
              ));
              return;
            }
            case Global::Kind::EXTERN: {
              frame.Set(mov, SymbolicValue::Pointer(
                  std::make_shared<SymbolicPointer>(&*::cast<Extern>(sym), off)
              ));
              return;
            }
            case Global::Kind::FUNC: {
              frame.Set(mov, SymbolicValue::Pointer(
                  std::make_shared<SymbolicPointer>(
                    heap_.Function(&*::cast<Func>(arg))
                  )
              ));
              return;
            }
            case Global::Kind::BLOCK: {
              frame.Set(mov, SymbolicValue::Pointer(
                  std::make_shared<SymbolicPointer>(&*::cast<Block>(arg))
              ));
              return;
            }
          }
          llvm_unreachable("invalid global kind");
        }
      }
      llvm_unreachable("invalid expression kind");
    }
    case Value::Kind::CONST: {
      auto &c = *::cast<Constant>(arg);
      switch (c.GetKind()) {
        case Constant::Kind::INT: {
          switch (auto ty = mov.GetType()) {
            case Type::I8:
            case Type::I16:
            case Type::I32:
            case Type::I64:
            case Type::V64:
            case Type::I128: {
              auto &ci = static_cast<ConstantInt &>(c);
              auto width = GetSize(ty) * 8;
              auto value = ci.GetValue();
              if (width != value.getBitWidth()) {
                frame.Set(mov, SymbolicValue::Integer(value.trunc(width)));
              } else {
                frame.Set(mov, SymbolicValue::Integer(value));
              }
              return;
            }
            case Type::F32:
            case Type::F64:
            case Type::F80:
            case Type::F128: {
              // TODO: produce a more accurate value.
              frame.Set(mov, SymbolicValue::Scalar());
              return;
            }
          }
          llvm_unreachable("invalid integer type");
        }
        case Constant::Kind::FLOAT: {
          llvm_unreachable("not implemented");
        }
      }
      llvm_unreachable("invalid constant kind");
    }
  }
  llvm_unreachable("invalid value kind");
}

// -----------------------------------------------------------------------------
bool SymbolicApprox::Malloc(CallSite &call, const std::optional<APInt> &size)
{
  auto &frame = *ctx_.GetActiveFrame();
  auto orig = std::make_pair(frame.GetIndex(), call.GetSubValue(0));
  if (size) {
    auto ptr = ctx_.Malloc(call, size->getZExtValue());
    LLVM_DEBUG(llvm::dbgs() << "\t\tptr: " << *ptr << "\n");
    return frame.Set(call, SymbolicValue::Nullable(ptr, orig));
  } else {
    auto ptr = ctx_.Malloc(call, std::nullopt);
    LLVM_DEBUG(llvm::dbgs() << "\t\tptr: " << *ptr << "\n");
    return frame.Set(call, SymbolicValue::Nullable(ptr, orig));
  }
}

// -----------------------------------------------------------------------------
bool SymbolicApprox::Realloc(
    CallSite &call,
    const SymbolicValue &ptr,
    const std::optional<APInt> &size)
{
  auto &frame = *ctx_.GetActiveFrame();
  if (size) {
    auto ptr = ctx_.Malloc(call, size->getZExtValue());
    LLVM_DEBUG(llvm::dbgs() << "\t\tptr: " << *ptr << "\n");
    return frame.Set(call, SymbolicValue::Nullable(ptr));
  } else {
    auto ptr = ctx_.Malloc(call, std::nullopt);
    LLVM_DEBUG(llvm::dbgs() << "\t\tptr: " << *ptr << "\n");
    return frame.Set(call, SymbolicValue::Nullable(ptr));
  }
}
