// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "passes/local_const/builder.h"
#include "core/cast.h"
#include "core/func.h"
#include "core/inst.h"



// -----------------------------------------------------------------------------
static std::optional<int64_t> GetConstant(Inst *inst)
{
  if (auto *movInst = ::dyn_cast_or_null<MovInst>(inst)) {
    if (auto *value = ::dyn_cast_or_null<ConstantInt>(movInst->GetArg())) {
      if (value->GetValue().getMinSignedBits() <= 64) {
        return value->GetInt();
      }
    }
  }
  return {};
}

// -----------------------------------------------------------------------------
static bool IsExtern(const Global *global) {
  if (global->Is(Global::Kind::BLOCK)) {
    return false;
  }
  std::string_view name = global->GetName();
  if (name == "caml_local_roots") {
    return false;
  }
  if (name == "caml_bottom_of_stack") {
    return false;
  }
  if (name == "caml_last_return_address") {
    return false;
  }
  return true;
}

// -----------------------------------------------------------------------------
GraphBuilder::GraphBuilder(
    LCGraph &graph,
    Queue<LCSet> &queue,
    NodeMap &nodes,
    ID<LCSet> ext)
  : graph_(graph)
  , queue_(queue)
  , nodes_(nodes)
  , frame_(nullptr)
  , empty_(graph_.Set()->GetID())
  , externAlloc_(graph_.Alloc({}, 0)->GetID())
  , extern_(ext)
{
  // Set up the external node and push it to the queue.
  {
    LCSet *externSet = graph_.Find(extern_);
    externSet->AddRange(graph_.Find(externAlloc_));
    externSet->Range(externSet);

    LCDeref *externDeref = externSet->Deref();
    externSet->Edge(externDeref);
    externDeref->Edge(externSet);

    queue_.Push(extern_);
  }
}

// -----------------------------------------------------------------------------
GraphBuilder::~GraphBuilder()
{
  for (PhiInst *phi : phis_) {
    FixupPhi(*phi);
  }
}

// -----------------------------------------------------------------------------
void GraphBuilder::BuildCall(Inst &inst)
{
  switch (inst.GetKind()) {
    case Inst::Kind::CALL:
      BuildCall(static_cast<CallSite<ControlInst> &>(inst));
      return;
    case Inst::Kind::INVOKE: {
      BuildCall(static_cast<CallSite<TerminatorInst> &>(inst));
      return;
    }
    case Inst::Kind::TINVOKE:
    case Inst::Kind::TCALL: {
      if (auto *s = BuildCall(static_cast<CallSite<TerminatorInst> &>(inst))) {
        Map(&inst, Return(s));
      }
      return;
    }
    default: {
      llvm_unreachable("not a call instruction");
    }
  }
}

// -----------------------------------------------------------------------------
void GraphBuilder::BuildReturn(ReturnInst &inst)
{
  if (auto *set = Get(inst.GetValue())) {
    Map(&inst, Return(set));
  }
}

// -----------------------------------------------------------------------------
void GraphBuilder::BuildFrame(FrameInst &inst)
{
  /*
  const uint64_t size = inst.getParent()->getParent()->GetStackSize();
  unsigned idx = static_cast<FrameInst &>(inst).GetIdx();
  frame_ = frame_ ? frame_ : graph_.Alloc(size, size);

  Map(&inst, graph_.Find(frameCache_(idx, [this, idx] {
    LCSet *set = graph_.Set();
    set->AddElement(frame_, frame_->GetIndex(idx));
    queue_.Push(set->GetID());
    return set->GetID();
  })));
  */
  abort();
}

// -----------------------------------------------------------------------------
void GraphBuilder::BuildArg(ArgInst &arg)
{
  if (!IsPointerType(arg.GetType())) {
    return;
  }
  Map(&arg, graph_.Find(extern_));
}

// -----------------------------------------------------------------------------
void GraphBuilder::BuildLoad(LoadInst &load)
{
  if (!IsPointerType(load.GetType(0)) || load.GetLoadSize() != 8) {
    return;
  }

  auto addr = Get(load.GetAddr());
  assert(addr && "missing address to load from");
  Map(&load, Load(addr));
}

// -----------------------------------------------------------------------------
void GraphBuilder::BuildStore(StoreInst &store)
{
  if (store.GetStoreSize() != 8) {
    return;
  }
  if (auto value = Get(store.GetVal())) {
    auto addr = Get(store.GetAddr());
    assert(addr && "missing address to store to");
    Store(value, addr);
  }
}

// -----------------------------------------------------------------------------
void GraphBuilder::BuildFlow(BinaryInst &inst)
{
  if (!IsPointerType(inst.GetType(0))) {
    return;
  }

  auto lhs = Get(inst.GetLHS());
  auto rhs = Get(inst.GetRHS());
  if (lhs && rhs) {
    Map(&inst, Union(lhs, rhs));
  } else if (lhs) {
    Map(&inst, lhs);
  } else if (rhs) {
    Map(&inst, rhs);
  }
}

// -----------------------------------------------------------------------------
void GraphBuilder::BuildExtern(Inst &inst, Global *global)
{
  Map(&inst, IsExtern(global) ? graph_.Find(extern_) : graph_.Find(empty_));
}

// -----------------------------------------------------------------------------
void GraphBuilder::BuildMove(Inst &inst, Inst *arg)
{
  if (auto *set = Get(arg)) {
    Map(&inst, set);
  }
}

// -----------------------------------------------------------------------------
void GraphBuilder::BuildPhi(PhiInst &inst)
{
  if (IsPointerType(inst.GetType(0))) {
    Map(&inst, graph_.Set());
  }
  phis_.push_back(&inst);
}

// -----------------------------------------------------------------------------
void GraphBuilder::BuildAdd(AddInst &inst)
{
  if (!IsPointerType(inst.GetType())) {
    return;
  }

  auto lhsSet = Get(inst.GetLHS());
  auto rhsSet = Get(inst.GetRHS());

  // If there is an offset, create a node for it.
  if (auto lhs = GetConstant(inst.GetLHS()); rhsSet) {
    Map(&inst, Offset(rhsSet, *lhs));
    return;
  }
  if (auto rhs = GetConstant(inst.GetRHS()); lhsSet) {
    Map(&inst, Offset(lhsSet, *rhs));
    return;
  }

  // Otherwise, propagate both arguments.
  if (lhsSet && rhsSet) {
    Map(&inst, Range(Union(lhsSet, rhsSet)));
  } else if (lhsSet && !rhsSet) {
    Map(&inst, Range(lhsSet));
  } else if (rhsSet && !lhsSet) {
    Map(&inst, Range(rhsSet));
  }
}

// -----------------------------------------------------------------------------
void GraphBuilder::BuildSub(SubInst &inst)
{
  if (!IsPointerType(inst.GetType())) {
    return;
  }

  // If there is an offset, create a node for it.
  if (auto lhsSet = Get(inst.GetLHS())) {
    if (auto rhs = GetConstant(inst.GetRHS())) {
      Map(&inst, Offset(lhsSet, -*rhs));
    } else {
      // Otherwise, build a range node.
      Map(&inst, Range(lhsSet));
    }
  }
}

// -----------------------------------------------------------------------------
void GraphBuilder::BuildAlloca(AllocaInst &inst)
{
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void GraphBuilder::BuildXchg(ExchangeInst &xchg)
{
  if (!IsPointerType(xchg.GetType())) {
    return;
  }

  auto *addr = Get(xchg.GetAddr());
  assert(addr && "missing xchg address");

  if (auto *value = Get(xchg.GetVal())) {
    Store(value, addr);
    Map(&xchg, Load(addr));
  }
}

// -----------------------------------------------------------------------------
void GraphBuilder::BuildVAStart(VAStartInst &inst)
{
  auto *externSet = graph_.Find(extern_);
  Store(externSet, Range(Get(inst.GetVAList())));
}

// -----------------------------------------------------------------------------
void GraphBuilder::BuildSelect(SelectInst &si)
{
  LCSet *trueSet = Get(si.GetTrue());
  LCSet *falseSet = Get(si.GetFalse());

  if (trueSet && falseSet) {
    Map(&si, Union(trueSet, falseSet));
  } else if (trueSet) {
    Map(&si, trueSet);
  } else if (falseSet) {
    Map(&si, falseSet);
  }
}

// -----------------------------------------------------------------------------
template<typename T>
LCSet *GraphBuilder::BuildCall(CallSite<T> &call)
{
  if (auto *movInst = ::dyn_cast_or_null<MovInst>(call.GetCallee())) {
    if (auto *callee = ::dyn_cast_or_null<Global>(movInst->GetArg())) {
      // If the target is a known callee, figure out the size or substitute it
      // with a sensible default value, which is 128 bytes. All values stored
      // outside the fixed range are stored in a separate out-of-bounds set.
      const auto &name = callee->getName();
      if (name.substr(0, 10) == "caml_alloc") {
        const auto &k = name.substr(10);
        if (k == "1") {
          return Map(&call, Alloc(8, 16));
        }
        if (k == "2") {
          return Map(&call, Alloc(8, 24));
        }
        if (k == "3") {
          return Map(&call, Alloc(8, 32));
        }
        if (k == "N") {
          if (auto n = GetConstant(*call.arg_begin())) {
            return Map(&call, Alloc(8, *n));
          } else {
            return Map(&call, Alloc(8, std::nullopt));
          }
        }
        if (k == "_young" || k == "_small") {
          if (auto n = GetConstant(*call.arg_begin())) {
            return Map(&call, Alloc(8, *n * 8 + 8));
          } else {
            return Map(&call, Alloc(8, std::nullopt));
          }
        }
      }
      if (name == "malloc") {
        if (auto n = GetConstant(*call.arg_begin())) {
          return Map(&call, Alloc(0, *n));
        } else {
          return Map(&call, Alloc(0, std::nullopt));
        }
      }
    }
  }

  // If the call is not an allocation, propagate the arguments into the extern
  // node and let them flow out from the return value, back into the program.
  LCSet *externSet = graph_.Find(extern_);
  for (const Inst *arg : call.args()) {
    if (LCSet *argNode = Get(arg)) {
      argNode->Edge(externSet);
    }
  }
  if (auto ty = call.GetType(); IsPointerType(*ty)) {
    return Map(&call, externSet);
  }
  return nullptr;
}

// -----------------------------------------------------------------------------
void GraphBuilder::FixupPhi(PhiInst &inst) {
  if (LCSet *phiSet = Get(&inst)) {
    for (unsigned i = 0, n = inst.GetNumIncoming(); i < n; ++i) {
      auto *value = inst.GetValue(i);
      switch (value->GetKind()) {
        case Value::Kind::INST: {
          if (auto set = Get(static_cast<const Inst *>(value))) {
            set->Edge(phiSet);
          }
          break;
        }
        case Value::Kind::GLOBAL: {
          if (IsExtern(static_cast<const Global *>(value))) {
            phiSet->AddRange(graph_.Find(externAlloc_));
            queue_.Push(phiSet->GetID());
          }
          break;
        }
        case Value::Kind::EXPR: {
          switch (static_cast<Expr *>(value)->GetKind()) {
            case Expr::Kind::SYMBOL_OFFSET: {
              auto *e = static_cast<SymbolOffsetExpr *>(value);
              if (IsExtern(e->GetSymbol())) {
                phiSet->AddRange(graph_.Find(externAlloc_));
                queue_.Push(phiSet->GetID());
              }
              break;
            }
          }
          break;
        }
        case Value::Kind::CONST: {
          break;
        }
      }
    }
  }
}

// -----------------------------------------------------------------------------
LCSet *GraphBuilder::Alloc(uint64_t index, const std::optional<uint64_t> &size)
{
  LCSet *set = graph_.Set();
  LCAlloc *alloc = graph_.Alloc(size, 16 * 8);
  set->AddElement(alloc, alloc->GetIndex(index));
  queue_.Push(set->GetID());
  return set;
}

// -----------------------------------------------------------------------------
LCSet *GraphBuilder::Return(LCSet *set)
{
  // Create a node that makes all pointers live.
  LCSet *ret = graph_.Set();
  ret->Range(ret);
  ret->Deref()->Edge(ret);

  // Direct all pointers to it.
  set->Edge(ret);

  // Inst will be mapped to it.
  return ret;
}

// -----------------------------------------------------------------------------
void GraphBuilder::Store(LCSet *from, LCSet *to)
{
  from->Edge(to->Deref());
}

// -----------------------------------------------------------------------------
LCSet *GraphBuilder::Load(LCSet *set)
{
  return graph_.Find(loadCache_(set->GetID(), [this, set] {
    LCSet *result = graph_.Set();
    set->Deref()->Edge(result);
    return result->GetID();
  }));
}

// -----------------------------------------------------------------------------
LCSet *GraphBuilder::Offset(LCSet *set, int64_t offset)
{
  return graph_.Find(offsetCache_(set->GetID(), offset, [this, set, offset] {
    LCSet *result = graph_.Set();
    set->Offset(result, offset);
    return result->GetID();
  }));
}

// -----------------------------------------------------------------------------
LCSet *GraphBuilder::Union(LCSet *a, LCSet *b)
{
  return graph_.Find(unionCache_(a->GetID(), b->GetID(), [this, a, b] {
    LCSet *result = graph_.Set();
    a->Edge(result);
    b->Edge(result);
    return result->GetID();
  }));
}

// -----------------------------------------------------------------------------
LCSet *GraphBuilder::Range(LCSet *set)
{
  return graph_.Find(rangeCache_(set->GetID(), [this, set] {
    LCSet *range = graph_.Set();
    set->Range(range);
    return range->GetID();
  }));
}
