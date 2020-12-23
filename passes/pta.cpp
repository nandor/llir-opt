// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <cstdlib>

#include <memory>
#include <unordered_map>
#include <unordered_set>

#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/ADT/SmallPtrSet.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/cfg.h"
#include "core/constant.h"
#include "core/data.h"
#include "core/func.h"
#include "core/insts.h"
#include "core/inst_visitor.h"
#include "core/prog.h"
#include "passes/pta.h"

#include "pta/node.h"
#include "pta/solver.h"



// -----------------------------------------------------------------------------
char AnalysisID<PointsToAnalysis>::ID;

// -----------------------------------------------------------------------------
const char *PointsToAnalysis::kPassID = "pta";

// -----------------------------------------------------------------------------
const char *PointsToAnalysis::GetPassName() const
{
  return "Points-To Analysis";
}


// -----------------------------------------------------------------------------
static std::optional<int64_t> ToInteger(Ref<Inst> inst)
{
  if (auto movInst = ::cast_or_null<MovInst>(inst)) {
    if (auto intConst = ::cast_or_null<ConstantInt>(movInst->GetArg())) {
      if (intConst->GetValue().getMinSignedBits() >= 64) {
        return intConst->GetInt();
      }
    }
  }
  return std::nullopt;
}

// -----------------------------------------------------------------------------
Global *ToGlobal(Ref<Inst> inst)
{
  if (auto movInst = ::cast_or_null<MovInst>(inst)) {
    if (auto global = ::cast_or_null<Global>(movInst->GetArg())) {
      return &*global;
    }
  }
  return nullptr;
}

// -----------------------------------------------------------------------------
class PTAContext final {
public:
  /// Initialises the context, scanning globals.
  PTAContext(Prog &prog);

  /// Explores the call graph starting from a function.
  void Explore(Func *func)
  {
    queue_.emplace_back(std::vector<Inst *>{}, func);
    while (!queue_.empty()) {
      while (!queue_.empty()) {
        auto [cs, func] = queue_.back();
        queue_.pop_back();
        Builder(*this, cs, *func).Build();
      }
      solver_.Solve();

      for (auto &func : Expand()) {
        queue_.push_back(func);
      }
    }
  }

  /// Checks if a function can be invoked.
  bool Reachable(Func *func) const
  {
    return explored_.count(func) != 0;
  }

private:
  /// Arguments & return values to a function.
  struct FunctionContext {
    /// Argument sets.
    std::vector<RootNode *> Args;
    /// Return set.
    std::vector<RootNode *> Returns;
    /// Frame for dynamic allocations.
    RootNode *Alloca;
    /// Individual objects in the frame.
    llvm::DenseMap<unsigned, RootNode *> Frame;
    /// Variable argument glob.
    RootNode *VA;
    /// True if function was expanded.
    bool Expanded;
  };

  /// Call site information.
  struct CallContext {
    /// Call context.
    std::vector<Inst *> Context;
    /// Called function.
    RootNode *Callee;
    /// Arguments to call.
    std::vector<RootNode *> Args;
    /// Return values from the call.
    std::vector<RootNode *> Returns;
    /// Expanded callees at this site.
    std::unordered_set<Func *> ExpandedFuncs;
    /// Expanded externs at this site.
    std::unordered_set<Extern *> ExpandedExterns;

    CallContext(
        const std::vector<Inst *> &context,
        RootNode *callee,
        llvm::ArrayRef<RootNode *> args,
        llvm::ArrayRef<RootNode *> rets)
      : Context(context)
      , Callee(callee)
      , Args(args.begin(), args.end())
    {
    }
  };

  /// Class for call strings.
  using CallString = std::vector<Inst *>;

  /// Helper class to build constraints.
  class Builder final : public InstVisitor<void> {
  public:
    Builder(
        PTAContext &ctx,
        const CallString &cs,
        Func &func)
      : ctx_(ctx)
      , cs_(cs)
      , func_(func)
      , fs_(ctx_.BuildFunction(cs_, func_))
    {
    }

    void Build();

  private:
    void VisitInst(Inst &inst) override { }
    void VisitUnaryInst(UnaryInst &i) override { }
    void VisitOverflowInst(OverflowInst &i) override { }
    void VisitDivisionInst(DivisionInst &i) override { }
    void VisitCmpInst(CmpInst &i) override { }

    void VisitBinaryInst(BinaryInst &i) override;
    void VisitCallSite(CallSite &i) override;
    void VisitReturnInst(ReturnInst &i) override;
    void VisitRaiseInst(RaiseInst &i) override;
    void VisitLandingPadInst(LandingPadInst &i) override;
    void VisitMemoryLoadInst(MemoryLoadInst &i) override;
    void VisitMemoryStoreInst(MemoryStoreInst &i) override;
    void VisitMemoryExchangeInst(MemoryExchangeInst &i) override;
    void VisitArgInst(ArgInst &i) override;
    void VisitMovInst(MovInst &i) override;
    void VisitPhiInst(PhiInst &i) override;
    void VisitSelectInst(SelectInst &i) override;
    void VisitAllocaInst(AllocaInst &i) override;
    void VisitFrameInst(FrameInst &i) override;
    void VisitVaStartInst(VaStartInst &i) override;
    void VisitCloneInst(CloneInst &i) override;

  private:
    /// Builds constraints for a call.
    std::vector<Node *> BuildCall(CallSite &call);
    /// Builds constraints for an allocation.
    std::optional<std::vector<Node *>> BuildAlloc(
        CallSite &call,
        const CallString &cs,
        const std::string_view name
    );

    /// Adds a new mapping.
    void Map(Ref<Inst> inst, Node *c)
    {
      if (c) {
        values_[inst] = c;
      }
    }

    /// Finds a constraint for an instruction.
    Node *Lookup(Ref<Inst> inst)
    {
      return values_[inst];
    }

    /// Builds the union of two nodes.
    Node *Union(Node *a, Node *b)
    {
      if (!a) {
        return b;
      }
      if (!b) {
        return a;
      }
      std::pair<Node *, Node *> key(a, b);
      auto it = unions_.emplace(key, nullptr);
      if (it.second) {
        auto *node = ctx_.solver_.Set();
        ctx_.solver_.Subset(a, node);
        ctx_.solver_.Subset(b, node);
        it.first->second = node;
      }
      return it.first->second;
    }

  private:
    /// Underlying context.
    PTAContext &ctx_;
    /// Call string.
    const CallString &cs_;
    /// Current function.
    Func &func_;
    /// Information about the current function.
    FunctionContext &fs_;
    /// Mapping from instructions to constraints.
    std::unordered_map<Ref<Inst>, Node *> values_;
    /// Cache of unions.
    std::unordered_map<std::pair<Node *, Node *>, Node *> unions_;
  };

private:
  /// Returns the constraints attached to a function.
  FunctionContext &BuildFunction(const std::vector<Inst *> &calls, Func &func);
  /// Simplifies the whole batch.
  std::vector<std::pair<std::vector<Inst *>, Func *>> Expand();
  /// Find the node containing a pointer to a global object.
  RootNode *Lookup(Global *g);

private:
  friend class Builder;

  /// Mapping from atoms to their nodes.
  std::unordered_map<Object *, RootNode *> objects_;
  /// Global variables.
  std::unordered_map<Global *, RootNode *> globals_;
  /// Node representing external values.
  RootNode *extern_;
  /// Function argument/return constraints.
  std::map<Func *, std::unique_ptr<FunctionContext>> funcs_;
  /// Call sites.
  std::vector<CallContext> calls_;
  /// Set of explored constraints.
  ConstraintSolver solver_;
  /// Work queue for functions to explore.
  std::vector<std::pair<std::vector<Inst *>, Func *>> queue_;
  /// Set of explored functions.
  std::unordered_set<Func *> explored_;
  /// Functions explored from the extern set.
  std::set<Func *> externCallees_;
  /// Buckets for exceptions.
  std::vector<RootNode *> exception_;
};

// -----------------------------------------------------------------------------
PTAContext::PTAContext(Prog &prog)
{
  // Set up the extern node.
  extern_ = solver_.Root();
  solver_.Subset(solver_.Load(extern_), extern_);

  // Set up atoms by creating a node for each object and
  // storing all the referenced objects in the atom.
  for (Data &data : prog.data()) {
    for (Object &object : data) {
      for (Atom &atom : object) {
        RootNode *node = Lookup(&atom);
        for (Item &item : atom) {
          if (auto *expr = item.AsExpr()) {
            switch (expr->GetKind()) {
              case Expr::Kind::SYMBOL_OFFSET: {
                auto *g = static_cast<SymbolOffsetExpr *>(expr)->GetSymbol();
                solver_.Store(node, Lookup(g));
                continue;
              }
            }
            llvm_unreachable("invalid expression kind");
          }
        }
      }
    }
  }
}

// -----------------------------------------------------------------------------
PTAContext::FunctionContext &
PTAContext::BuildFunction(const std::vector<Inst *> &calls, Func &func)
{
  auto key = &func;
  auto it = funcs_.emplace(key, nullptr);
  if (it.second) {
    it.first->second = std::make_unique<FunctionContext>();
    auto f = it.first->second.get();
    f->VA = solver_.Root();
    f->Alloca = solver_.Root(solver_.Set());
    for (auto &arg : func.params()) {
      f->Args.push_back(solver_.Root());
    }
    f->Expanded = false;
  }
  return *it.first->second;
}

// -----------------------------------------------------------------------------
std::vector<std::pair<std::vector<Inst *>, Func *>> PTAContext::Expand()
{
  std::vector<std::pair<std::vector<Inst *>, Func *>> callees;
  for (auto &call : calls_) {
    for (auto id : call.Callee->Set()->points_to_func()) {
      auto *func = solver_.Map(id);

      // Expand each call site only once.
      if (!call.ExpandedFuncs.insert(func).second) {
        continue;
      }

      // Call to be expanded, with context.
      callees.emplace_back(call.Context, func);

      // Connect arguments and return value.
      auto &funcSet = BuildFunction(call.Context, *func);
      for (unsigned i = 0; i < call.Args.size(); ++i) {
        if (auto *arg = call.Args[i]) {
          if (i >= funcSet.Args.size()) {
            if (func->IsVarArg()) {
              solver_.Subset(arg, funcSet.VA);
            }
          } else {
            solver_.Subset(arg, funcSet.Args[i]);
          }
        }
      }
      for (unsigned i = 0, n = call.Returns.size(); i < n; ++i) {
        if (funcSet.Returns.size() <= i) {
          funcSet.Returns.push_back(solver_.Root());
        }
        solver_.Subset(funcSet.Returns[i], call.Returns[i]);
      }
    }

    for (auto id : call.Callee->Set()->points_to_ext()) {
      auto *ext = solver_.Map(id);

      // Expand each call site only once.
      if (!call.ExpandedExterns.insert(ext).second) {
        continue;
      }

      // Connect arguments and return value.
      for (auto *set : call.Args) {
        if (set) {
          solver_.Subset(set, extern_);
        }
      }
      for (auto *set : call.Returns) {
        if (set) {
          solver_.Subset(extern_, set);
        }
      }
    }
  }

  // Look at the extern set - call all funcs which reach it.
  for (auto id : extern_->Set()->points_to_func()) {
    auto *func = solver_.Map(id);

    // Expand each call site only once.
    if (!externCallees_.insert(func).second) {
      continue;
    }

    // Call to be expanded, with context.
    callees.emplace_back(std::vector<Inst *>{}, func);

    // Connect arguments and return value.
    auto &funcSet = BuildFunction({}, *func);
    for (unsigned i = 0; i< func->params().size(); ++i) {
      solver_.Subset(extern_, funcSet.Args[i]);
    }
    for (auto *set : funcSet.Returns) {
      solver_.Subset(set, extern_);
    }
  }

  return callees;
}

// -----------------------------------------------------------------------------
RootNode *PTAContext::Lookup(Global *g)
{
  auto it = globals_.emplace(g, nullptr);
  if (!it.second) {
    return it.first->second;
  }

  auto *set = solver_.Set();
  auto *node = solver_.Root(set);
  it.first->second = node;

  switch (g->GetKind()) {
    case Global::Kind::EXTERN: {
      auto *ext = static_cast<Extern *>(g);
      set->AddExtern(solver_.Map(ext));
      return node;
    }
    case Global::Kind::FUNC: {
      auto *func = static_cast<Func *>(g);
      set->AddFunc(solver_.Map(func));
      return node;
    }
    case Global::Kind::BLOCK: {
      return node;
    }
    case Global::Kind::ATOM: {
      auto *atom = static_cast<Atom *>(g);
      auto at = objects_.emplace(atom->getParent(), nullptr);
      if (at.second) {
        at.first->second = solver_.Root();
      }
      set->AddNode(at.first->second->Set()->GetID());
      return node;
    }
  }
  llvm_unreachable("invalid global kind");
}

// -----------------------------------------------------------------------------
void PTAContext::Builder::Build()
{
  // Constraint sets for the function.
  auto &funcSet = ctx_.BuildFunction(cs_, func_);
  if (funcSet.Expanded) {
    return;
  }
  funcSet.Expanded = true;

  // Mark the function as explored.
  ctx_.explored_.insert(&func_);

  // For each instruction, generate a constraint.
  for (auto *block : llvm::ReversePostOrderTraversal<Func *>(&func_)) {
    for (auto &inst : *block) {
      Dispatch(inst);
    }
  }

  // Fixups for PHI nodes.
  for (auto &block : func_) {
    for (auto &phi : block.phis()) {
      std::set<Node *> ins;
      for (unsigned i = 0; i < phi.GetNumIncoming(); ++i) {
        if (auto *c = Lookup(phi.GetValue(i))) {
          ins.insert(c);
        }
      }

      auto *pc = Lookup(&phi);
      for (auto *c : ins) {
        ctx_.solver_.Subset(c, pc);
      }
    }
  }
}

// -----------------------------------------------------------------------------
void PTAContext::Builder::VisitBinaryInst(BinaryInst &i)
{
  if (auto *c = Union(Lookup(i.GetLHS()), Lookup(i.GetRHS()))) {
    Map(i, c);
  }
}

// -----------------------------------------------------------------------------
void PTAContext::Builder::VisitCallSite(CallSite &call)
{
  auto returns = BuildCall(call);
  if (returns.empty()) {
    return;
  }

  assert(returns.size() >= call.type_size() && "invalid return values");
  auto &solver = ctx_.solver_;
  for (unsigned i = 0, n = call.type_size(); i < n; ++i) {
    if (!returns[i]) {
      continue;
    }

    if (call.IsReturn()) {
      if (fs_.Returns.size() <= i) {
        fs_.Returns.push_back(solver.Root());
      }
      solver.Subset(returns[i], fs_.Returns[i]);
    } else {
      Map(call.GetSubValue(i), returns[i]);
    }
  }
}

// -----------------------------------------------------------------------------
std::vector<Node *> PTAContext::Builder::BuildCall(CallSite &call)
{
  CallString callString(cs_);
  callString.push_back(&call);

  if (auto *global = ToGlobal(call.GetCallee())) {
    switch (global->GetKind()) {
      case Global::Kind::FUNC: {
        auto &callee = static_cast<Func &>(*global);
        if (auto c = BuildAlloc(call, callString, callee.GetName())) {
          // If the function is an allocation site, stop and
          // record it. Otherwise, recursively traverse callees.
          ctx_.explored_.insert(&func_);
          return *c;
        } else {
          auto &funcSet = ctx_.BuildFunction(callString, callee);
          for (unsigned i = 0, n = call.arg_size(); i < n; ++i) {
            if (auto *c = Lookup(call.arg(i))) {
              if (funcSet.Args.size() <= i) {
                if (callee.IsVarArg()) {
                  ctx_.solver_.Subset(c, funcSet.VA);
                }
              } else {
                ctx_.solver_.Subset(c, funcSet.Args[i]);
              }
            }
          }
          ctx_.queue_.emplace_back(callString, &callee);
          for (unsigned i = 0, n = call.type_size(); i < n; ++i) {
            if (funcSet.Returns.size() <= i) {
              funcSet.Returns.push_back(ctx_.solver_.Root());
            }
          }
          return { funcSet.Returns.begin(), funcSet.Returns.end() };
        }
      }
      case Global::Kind::EXTERN: {
        auto &callee = static_cast<Extern &>(*global);
        if (auto c = BuildAlloc(call, callString, callee.GetName())) {
          return *c;
        } else {
          auto *externs = ctx_.extern_;
          for (Ref<Inst> arg : call.args()) {
            if (auto *c = Lookup(arg)) {
              ctx_.solver_.Subset(c, externs);
            }
          }
          std::vector<Node *> returns;
          for (unsigned i = 0, n = call.type_size(); i < n; ++i) {
            returns.push_back(externs);
          }
          return returns;
        }
      }
      case Global::Kind::BLOCK:
      case Global::Kind::ATOM: {
        llvm_unreachable("invalid callee");
      }
    }
    llvm_unreachable("invalid global kind");
  } else {
    // Indirect call - constraint to be expanded later.
    std::vector<RootNode *> argsRoot;
    for (Ref<Inst> arg : call.args()) {
      argsRoot.push_back(ctx_.solver_.Anchor(Lookup(arg)));
    }
    std::vector<RootNode *> retsRoot;
    std::vector<Node *> retsNode;
    for (unsigned i = 0, n = call.type_size(); i < n; ++i) {
      auto *node = ctx_.solver_.Root();
      retsRoot.push_back(node);
      retsNode.push_back(node);
    }
    ctx_.calls_.emplace_back(
        callString,
        ctx_.solver_.Anchor(Lookup(call.GetCallee())),
        argsRoot,
        retsRoot
    );
    return retsNode;
  }
}

// -----------------------------------------------------------------------------
static bool IsCamlAlloc(const std::string_view name)
{
  return name == "caml_alloc1"
      || name == "caml_alloc2"
      || name == "caml_alloc3"
      || name == "caml_allocN";
}

// -----------------------------------------------------------------------------
static bool IsMalloc(const std::string_view name)
{
  return name == "malloc"
      || name == "caml_alloc"
      || name == "caml_alloc_custom_mem"
      || name == "caml_alloc_dummy"
      || name == "caml_alloc_for_heap"
      || name == "caml_alloc_shr_aux"
      || name == "caml_alloc_small"
      || name == "caml_alloc_small_aux"
      || name == "caml_alloc_small_dispatch"
      || name == "caml_alloc_sprintf"
      || name == "caml_alloc_string"
      || name == "caml_alloc_tuple"
      || name == "caml_stat_alloc"
      || name == "caml_stat_alloc_noexc"
      || name == "caml_stat_alloc_aligned"
      || name == "caml_stat_alloc_aligned_noexc";
}

// -----------------------------------------------------------------------------
static bool IsRealloc(const std::string_view name)
{
  return name == "realloc"
      || name == "caml_stat_resize_noexc";
}

// -----------------------------------------------------------------------------
std::optional<std::vector<Node *>>
PTAContext::Builder::BuildAlloc(
    CallSite &call,
    const CallString &cs,
    const std::string_view name)
{
  if (IsCamlAlloc(name)) {
    std::vector<Node *> returns;
    if (call.arg_size() == 2) {
      assert(call.type_size() == 2 && "malformed caml_alloc");
      returns.push_back(Lookup(call.arg(0)));    // state
      returns.push_back(ctx_.solver_.Alloc(cs)); // new object
      return returns;
    } else {
      llvm_unreachable("not implemented");
    }
  }
  if (IsMalloc(name)) {
    std::vector<Node *> returns;
    returns.push_back(ctx_.solver_.Alloc(cs));
    return returns;
  }
  if (IsRealloc(name)) {
    std::vector<Node *> returns;
    returns.push_back(ctx_.solver_.Alloc(cs));
    return returns;
  }
  return {};
};

// -----------------------------------------------------------------------------
void PTAContext::Builder::VisitReturnInst(ReturnInst &ret)
{
  for (unsigned i = 0, n = ret.arg_size(); i < n; ++i) {
    if (auto *c = Lookup(ret.arg(i))) {
      if (fs_.Returns.size() <= i) {
        fs_.Returns.push_back(ctx_.solver_.Root());
      }
      ctx_.solver_.Subset(c, fs_.Returns[i]);
    }
  }
}

// -----------------------------------------------------------------------------
void PTAContext::Builder::VisitRaiseInst(RaiseInst &raise)
{
  for (unsigned i = 0, n = raise.arg_size(); i < n; ++i) {
    if (ctx_.exception_.size() <= i) {
      ctx_.exception_.push_back(ctx_.solver_.Root());
    }
    ctx_.solver_.Subset(Lookup(raise.arg(i)), ctx_.exception_[i]);
  }
}

// -----------------------------------------------------------------------------
void PTAContext::Builder::VisitLandingPadInst(LandingPadInst &pad)
{
  for (unsigned i = 0, n = pad.type_size(); i < n; ++i) {
    if (ctx_.exception_.size() <= i) {
      ctx_.exception_.push_back(ctx_.solver_.Root());
    }
    Map(pad.GetSubValue(i), ctx_.exception_[i]);
  }
}

// -----------------------------------------------------------------------------
void PTAContext::Builder::VisitMemoryLoadInst(MemoryLoadInst &i)
{
  if (auto *addr = Lookup(i.GetAddr())) {
    Map(i, ctx_.solver_.Load(addr));
  }
}

// -----------------------------------------------------------------------------
void PTAContext::Builder::VisitMemoryStoreInst(MemoryStoreInst &i)
{
  if (auto *value = Lookup(i.GetValue())) {
    if (auto *addr = Lookup(i.GetAddr())) {
      ctx_.solver_.Store(addr, value);
    }
  }
}

// -----------------------------------------------------------------------------
void PTAContext::Builder::VisitMemoryExchangeInst(MemoryExchangeInst &i)
{
  auto *addr = Lookup(i.GetAddr());
  if (auto *value = Lookup(i.GetValue())) {
    ctx_.solver_.Store(addr, value);
  }
  Map(i, ctx_.solver_.Load(addr));
}

// -----------------------------------------------------------------------------
void PTAContext::Builder::VisitArgInst(ArgInst &i)
{
  unsigned idx = i.GetIndex();
  assert(idx < fs_.Args.size() && "argument out of range");
  Map(i, fs_.Args[idx]);
}

// -----------------------------------------------------------------------------
void PTAContext::Builder::VisitMovInst(MovInst &i)
{
  auto arg = i.GetArg();
  switch (arg->GetKind()) {
    case Value::Kind::INST: {
      // Instruction - propagate.
      Map(i, Lookup(&*::cast<Inst>(arg)));
      return;
    }
    case Value::Kind::GLOBAL: {
      // Global - set with global.
      Map(i, ctx_.Lookup(&*::cast<Global>(arg)));
      return;
    }
    case Value::Kind::EXPR: {
      auto &expr = *::cast<Expr>(arg);
      // Expression - set with offset.
      switch (expr.GetKind()) {
        case Expr::Kind::SYMBOL_OFFSET: {
          auto *sym = static_cast<SymbolOffsetExpr &>(expr).GetSymbol();
          Map(i, ctx_.Lookup(sym));
          return;
        }
      }
      llvm_unreachable("invalid expression kind");
    }
    case Value::Kind::CONST: {
      // Constant value - no constraint.
      return;
    }
  }
  llvm_unreachable("invalid value kind");
}

// -----------------------------------------------------------------------------
void PTAContext::Builder::VisitPhiInst(PhiInst &i)
{
  Map(i, ctx_.solver_.Empty());
}

// -----------------------------------------------------------------------------
void PTAContext::Builder::VisitSelectInst(SelectInst &i)
{
  auto *vt = Lookup(i.GetTrue());
  auto *vf = Lookup(i.GetFalse());
  if (auto *c = Union(vt, vf)) {
    Map(i, c);
  }
}

// -----------------------------------------------------------------------------
void PTAContext::Builder::VisitAllocaInst(AllocaInst &i)
{
  Map(i, fs_.Alloca);
}

// -----------------------------------------------------------------------------
void PTAContext::Builder::VisitFrameInst(FrameInst &i)
{
  const unsigned obj = i.GetObject();
  RootNode *node;
  if (auto it = fs_.Frame.find(obj); it != fs_.Frame.end()) {
    node = it->second;
  } else {
    node = ctx_.solver_.Root(ctx_.solver_.Set());
    fs_.Frame.insert({ obj, node });
  }
  Map(i, node);
}

// -----------------------------------------------------------------------------
void PTAContext::Builder::VisitVaStartInst(VaStartInst &i)
{
  if (auto *value = Lookup(i.GetVAList())) {
    ctx_.solver_.Subset(fs_.VA, value);
  }
}

// -----------------------------------------------------------------------------
void PTAContext::Builder::VisitCloneInst(CloneInst &clone)
{
  CallString callString(cs_);
  callString.push_back(&clone);

  std::vector<RootNode *> argsRoot, retsRoot;
  argsRoot.push_back(ctx_.solver_.Anchor(Lookup(clone.GetArg())));
  ctx_.calls_.emplace_back(
      callString,
      ctx_.solver_.Anchor(Lookup(clone.GetCallee())),
      argsRoot,
      retsRoot
  );
}

// -----------------------------------------------------------------------------
bool PointsToAnalysis::Run(Prog &prog)
{
  PTAContext graph(prog);

  for (auto &func : prog) {
    if (func.IsRoot()) {
      graph.Explore(&func);
    }
  }

  for (auto &func : prog) {
    if (graph.Reachable(&func)) {
      reachable_.insert(&func);
    }
  }

  return false;
}
