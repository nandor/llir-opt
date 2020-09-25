// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "passes/local_const/builder.h"
#include "passes/local_const/context.h"
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
GraphBuilder::GraphBuilder(LCContext &context, Func &func, Queue<LCSet> &queue)
  : context_(context)
  , func_(func)
  , graph_(context_.Graph())
  , queue_(queue)
  , empty_(graph_.Set()->GetID())
  , externAlloc_(graph_.Alloc({}, 0))
  , rootAlloc_(graph_.Alloc(8, 8))
{
  // Set up the external node and push it to the queue.
  {
    LCSet *externSet = context_.Extern();
    externSet->AddRange(externAlloc_);
    externSet->Range(externSet);

    LCDeref *externDeref = externSet->Deref();
    externSet->Edge(externDeref);
    externDeref->Edge(externSet);

    LCSet *rootSet = context_.Root();
    rootSet->AddElement(rootAlloc_, rootAlloc_->GetIndex(0));
    rootSet->Range(rootSet);
    rootSet->Deref()->Edge(rootSet);

    queue_.Push(rootSet->GetID());
    queue_.Push(externSet->GetID());
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
  if (inst.HasAnnot<CamlFrame>()) {
    if (!lva_) {
      lva_.reset(new LiveVariables(&func_));
    }
    LCSet *live = graph_.Set();
    live->Range(live);
    live->Deref()->Edge(live);
    for (auto *inst : lva_->LiveOut(&inst)) {
      if (inst->HasAnnot<CamlValue>()) {
        if (auto *set = context_.GetNode(inst)) {
          set->Edge(live);
        }
      }
    }
    context_.MapLive(&inst, live);
  }

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
        context_.MapNode(&inst, Return(s));
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
  if (auto *set = context_.GetNode(inst.GetValue())) {
    context_.MapNode(&inst, Return(set));
  }
}

// -----------------------------------------------------------------------------
void GraphBuilder::BuildFrame(FrameInst &inst)
{
  unsigned obj = inst.GetObject();
  LCAlloc *alloc = context_.Frame(obj);
  context_.MapNode(&inst, graph_.Find(frameCache_(obj, [this, alloc, &inst] {
    LCSet *set = graph_.Set();
    set->AddElement(alloc, alloc->GetIndex(inst.GetOffset()));
    queue_.Push(set->GetID());
    return set->GetID();
  })));
}

// -----------------------------------------------------------------------------
void GraphBuilder::BuildArg(ArgInst &arg)
{
  if (!IsPointerType(arg.GetType())) {
    return;
  }
  context_.MapNode(&arg, context_.Extern());
}

// -----------------------------------------------------------------------------
void GraphBuilder::BuildLoad(LoadInst &load)
{
  if (!IsPointerType(load.GetType(0))) {
    return;
  }

  if (auto *addr = context_.GetNode(load.GetAddr())) {
    context_.MapNode(&load, Load(addr));
  }
}

// -----------------------------------------------------------------------------
void GraphBuilder::BuildStore(StoreInst &store)
{
  if (!IsPointerType(store.GetVal()->GetType(0))) {
    return;
  }
  if (auto value = context_.GetNode(store.GetVal())) {
    auto addr = context_.GetNode(store.GetAddr());
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

  auto lhs = context_.GetNode(inst.GetLHS());
  auto rhs = context_.GetNode(inst.GetRHS());
  if (lhs && rhs) {
    context_.MapNode(&inst, Range(Union(lhs, rhs)));
  } else if (lhs) {
    context_.MapNode(&inst, Range(lhs));
  } else if (rhs) {
    context_.MapNode(&inst, Range(rhs));
  }
}

// -----------------------------------------------------------------------------
void GraphBuilder::BuildExtern(Inst &inst, Global *global)
{
  if (auto *set = GetGlobal(global)) {
    context_.MapNode(&inst, set);
  } else {
    context_.MapNode(&inst, graph_.Find(empty_));
  }
}

// -----------------------------------------------------------------------------
void GraphBuilder::BuildMove(Inst &inst, Inst *arg)
{
  if (auto *set = context_.GetNode(arg)) {
    context_.MapNode(&inst, set);
  }
}

// -----------------------------------------------------------------------------
void GraphBuilder::BuildPhi(PhiInst &inst)
{
  if (IsPointerType(inst.GetType(0))) {
    context_.MapNode(&inst, graph_.Set());
  }
  phis_.push_back(&inst);
}

// -----------------------------------------------------------------------------
void GraphBuilder::BuildAdd(AddInst &inst)
{
  if (!IsPointerType(inst.GetType())) {
    return;
  }

  auto lhsSet = context_.GetNode(inst.GetLHS());
  auto rhsSet = context_.GetNode(inst.GetRHS());

  // If there is an offset, create a node for it.
  if (auto lhs = GetConstant(inst.GetLHS()); lhs && rhsSet) {
    context_.MapNode(&inst, Offset(rhsSet, *lhs));
    return;
  }
  if (auto rhs = GetConstant(inst.GetRHS()); rhs && lhsSet) {
    context_.MapNode(&inst, Offset(lhsSet, *rhs));
    return;
  }

  // Otherwise, propagate both arguments.
  if (lhsSet && rhsSet) {
    context_.MapNode(&inst, Range(Union(lhsSet, rhsSet)));
  } else if (lhsSet && !rhsSet) {
    context_.MapNode(&inst, Range(lhsSet));
  } else if (rhsSet && !lhsSet) {
    context_.MapNode(&inst, Range(rhsSet));
  }
}

// -----------------------------------------------------------------------------
void GraphBuilder::BuildSub(SubInst &inst)
{
  if (!IsPointerType(inst.GetType())) {
    return;
  }

  // If there is an offset, create a node for it.
  if (auto lhsSet = context_.GetNode(inst.GetLHS())) {
    if (auto rhs = GetConstant(inst.GetRHS())) {
      context_.MapNode(&inst, Offset(lhsSet, -*rhs));
    } else {
      // Otherwise, build a range node.
      context_.MapNode(&inst, Range(lhsSet));
    }
  }
}

// -----------------------------------------------------------------------------
void GraphBuilder::BuildAlloca(AllocaInst &inst)
{
  if (!alloca_) {
    LCAlloc *alloc = graph_.Alloc({}, 0);
    LCSet *rootSet = graph_.Set();
    rootSet->AddElement(alloc, alloc->GetIndex(0));
    alloca_ = rootSet->GetID();
  }
  context_.MapNode(&inst, graph_.Find(*alloca_));
}

// -----------------------------------------------------------------------------
void GraphBuilder::BuildXchg(XchgInst &xchg)
{
  if (!IsPointerType(xchg.GetType())) {
    return;
  }

  auto *addr = context_.GetNode(xchg.GetAddr());
  assert(addr && "missing xchg address");

  if (auto *value = context_.GetNode(xchg.GetVal())) {
    Store(value, addr);
    context_.MapNode(&xchg, Load(addr));
  }
}

// -----------------------------------------------------------------------------
void GraphBuilder::BuildVAStart(VAStartInst &inst)
{
  auto *externSet = context_.Extern();
  Store(externSet, Range(context_.GetNode(inst.GetVAList())));
}

// -----------------------------------------------------------------------------
void GraphBuilder::BuildSelect(SelectInst &si)
{
  LCSet *trueSet = context_.GetNode(si.GetTrue());
  LCSet *falseSet = context_.GetNode(si.GetFalse());

  if (trueSet && falseSet) {
    context_.MapNode(&si, Union(trueSet, falseSet));
  } else if (trueSet) {
    context_.MapNode(&si, trueSet);
  } else if (falseSet) {
    context_.MapNode(&si, falseSet);
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
          return context_.MapNode(&call, Alloc(8, 16));
        }
        if (k == "2") {
          return context_.MapNode(&call, Alloc(8, 24));
        }
        if (k == "3") {
          return context_.MapNode(&call, Alloc(8, 32));
        }
        if (k == "N") {
          if (auto n = GetConstant(*call.arg_begin())) {
            return context_.MapNode(&call, Alloc(8, *n));
          } else {
            return context_.MapNode(&call, Alloc(8, std::nullopt));
          }
        }
        if (k == "_young" || k == "_small") {
          if (auto n = GetConstant(*call.arg_begin())) {
            return context_.MapNode(&call, Alloc(8, *n * 8 + 8));
          } else {
            return context_.MapNode(&call, Alloc(8, std::nullopt));
          }
        }
      }
      if (name == "malloc") {
        if (auto n = GetConstant(*call.arg_begin())) {
          return context_.MapNode(&call, Alloc(0, *n));
        } else {
          return context_.MapNode(&call, Alloc(0, std::nullopt));
        }
      }
    }
  }

  // If the call is not an allocation, propagate the arguments into the extern
  // node and let them flow out from the return value, back into the program.
  LCSet *externSet = context_.Extern();
  for (const Inst *arg : call.args()) {
    if (LCSet *argNode = context_.GetNode(arg)) {
      argNode->Edge(externSet);
    }
  }
  if (auto ty = call.GetType(); ty && IsPointerType(*ty)) {
    return context_.MapNode(&call, externSet);
  }
  return nullptr;
}

// -----------------------------------------------------------------------------
void GraphBuilder::FixupPhi(PhiInst &phi) {
  if (LCSet *phiSet = context_.GetNode(&phi)) {
    for (unsigned i = 0, n = phi.GetNumIncoming(); i < n; ++i) {
      if (auto set = context_.GetNode(phi.GetValue(i))) {
        set->Edge(phiSet);
        queue_.Push(set->GetID());
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

// -----------------------------------------------------------------------------
LCSet *GraphBuilder::GetGlobal(const Global *global)
{
  switch (global->GetKind()) {
    case Global::Kind::BLOCK:
    case Global::Kind::FUNC:
      return nullptr;
    case Global::Kind::ATOM:
    case Global::Kind::EXTERN: {
      std::string_view name = global->GetName();
      if (name == "caml_local_roots") {
        return context_.Root();
      }
      return context_.Extern();
    }
  }
  llvm_unreachable("invalid global type");
}
