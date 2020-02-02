// This file if part of the genm-opt project.
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
#include "core/prog.h"
#include "passes/pta.h"

#include "pta/node.h"
#include "pta/solver.h"



/**
 * Global context, building and solving constraints.
 */
class PTAContext final {
public:
  /// Initialises the context, scanning globals.
  PTAContext(Prog *prog);

  /// Explores the call graph starting from a function.
  void Explore(Func *func)
  {
    queue_.emplace_back(std::vector<Inst *>{}, func);
    while (!queue_.empty()) {
      while (!queue_.empty()) {
        auto [calls, func] = queue_.back();
        queue_.pop_back();
        BuildConstraints(calls, func);
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
    RootNode *Return;
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
    /// Return value.
    RootNode *Return;
    /// Expanded callees at this site.
    std::unordered_set<Func *> Expanded;

    CallContext(
        const std::vector<Inst *> &context,
        RootNode *callee,
        std::vector<RootNode *> args,
        RootNode *ret)
      : Context(context)
      , Callee(callee)
      , Args(args)
      , Return(ret)
    {
    }
  };

  /// Context for a function - mapping instructions to constraints.
  class LocalContext {
  public:
    /// Adds a new mapping.
    void Map(Inst &inst, Node *c)
    {
      if (c) {
        values_[&inst] = c;
      }
    }

    /// Finds a constraint for an instruction.
    Node *Lookup(Inst *inst)
    {
      return values_[inst];
    }

  private:
    /// Mapping from instructions to constraints.
    std::unordered_map<Inst *, Node *> values_;
  };

  /// Builds constraints for a single function.
  void BuildConstraints(const std::vector<Inst *> &calls, Func *func);
  /// Returns the constraints attached to a function.
  FunctionContext &BuildFunction(const std::vector<Inst *> &calls, Func *func);
  /// Builds a constraint for a single global.
  Node *BuildGlobal(Global *g);
  // Builds a constraint from a value.
  Node *BuildValue(LocalContext &ctx, Value *v);
  // Creates a constraint for a call.
  template<typename T>
  Node *BuildCall(
      const std::vector<Inst *> &calls,
      LocalContext &ctx,
      Inst *caller,
      Inst *callee,
      llvm::iterator_range<typename CallSite<T>::arg_iterator> &&args
  );
  // Creates a constraint for a potential allocation site.
  template<typename T>
  Node *BuildAlloc(
      LocalContext &ctx,
      const std::vector<Inst *> &calls,
      const std::string_view &name,
      llvm::iterator_range<typename CallSite<T>::arg_iterator> &args
  );

  /// Extracts an integer from a potential mov instruction.
  std::optional<int64_t> ToInteger(Inst *inst);
  /// Extracts a global from a potential mov instruction.
  Global *ToGlobal(Inst *inst);

  /// Simplifies the whole batch.
  std::vector<std::pair<std::vector<Inst *>, Func *>> Expand();

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
};

// -----------------------------------------------------------------------------
PTAContext::PTAContext(Prog *prog)
{
  std::vector<std::tuple<Atom *, Atom *>> fixups;
  std::unordered_map<Atom *, RootNode *> chunks;

  RootNode *chunk = nullptr;
  for (auto &data : prog->data()) {
    for (auto &atom : data) {
      chunk = chunk ? chunk : solver_.Root();
      solver_.Chunk(&atom, chunk);
      chunks.emplace(&atom, chunk);

      for (auto *item : atom) {
        switch (item->GetKind()) {
          case Item::Kind::INT8:    break;
          case Item::Kind::INT16:   break;
          case Item::Kind::INT32:   break;
          case Item::Kind::INT64:   break;
          case Item::Kind::FLOAT64: break;
          case Item::Kind::SPACE:   break;
          case Item::Kind::STRING:  break;
          case Item::Kind::ALIGN:   break;
          case Item::Kind::SYMBOL: {
            auto *global = item->GetSymbol();
            switch (global->GetKind()) {
              case Global::Kind::SYMBOL: {
                assert(!"not implemented");
                break;
              }
              case Global::Kind::EXTERN: {
                auto *ext = static_cast<Extern *>(global);
                solver_.Store(solver_.Lookup(&atom), solver_.Lookup(ext));
                break;
              }
              case Global::Kind::FUNC: {
                auto *func = static_cast<Func *>(global);
                solver_.Store(solver_.Lookup(&atom), solver_.Lookup(func));
                break;
              }
              case Global::Kind::BLOCK: {
                assert(!"not implemented");
                break;
              }
              case Global::Kind::ATOM: {
                fixups.emplace_back(static_cast<Atom *>(global), &atom);
                break;
              }
            }
            break;
          }
          case Item::Kind::END: {
            chunk = nullptr;
            break;
          }
        }
      }
    }
  }

  for (auto &fixup : fixups) {
    auto [item, atom] = fixup;
    solver_.Store(solver_.Lookup(atom), solver_.Lookup(item));
  }
  solver_.Solve();
}

// -----------------------------------------------------------------------------
void PTAContext::BuildConstraints(
    const std::vector<Inst *> &calls,
    Func *func)
{
  // Constraint sets for the function.
  auto &funcSet = BuildFunction(calls, func);
  if (funcSet.Expanded) {
    return;
  }
  funcSet.Expanded = true;

  // Mark the function as explored.
  explored_.insert(func);

  // Context storing local instruction - constraint mappings.
  LocalContext ctx;

  // For each instruction, generate a constraint.
  for (auto *block : llvm::ReversePostOrderTraversal<Func*>(func)) {
    for (auto &inst : *block) {
      switch (inst.GetKind()) {
        // Call - explore.
        case Inst::Kind::CALL: {
          auto &call = static_cast<CallInst &>(inst);
          auto *callee = call.GetCallee();
          if (auto *c = BuildCall<ControlInst>(calls, ctx, &inst, callee, call.args())) {
            ctx.Map(call, c);
          }
          break;
        }
        // Invoke Call - explore.
        case Inst::Kind::INVOKE: {
          auto &call = static_cast<InvokeInst &>(inst);
          auto *callee = call.GetCallee();
          if (auto *c = BuildCall<TerminatorInst>(calls, ctx, &inst, callee, call.args())) {
            ctx.Map(call, c);
          }
          break;
        }
        // Tail Call - explore.
        case Inst::Kind::TCALL:
        case Inst::Kind::TINVOKE: {
          auto &call = static_cast<CallSite<TerminatorInst>&>(inst);
          auto *callee = call.GetCallee();
          if (auto *c = BuildCall<TerminatorInst>(calls, ctx, &inst, callee, call.args())) {
            solver_.Subset(c, funcSet.Return);
          }
          break;
        }
        // Return - generate return constraint.
        case Inst::Kind::RET: {
          auto &retInst = static_cast<ReturnInst &>(inst);
          if (auto *val = retInst.GetValue()) {
            if (auto *c = ctx.Lookup(val)) {
              solver_.Subset(c, funcSet.Return);
            }
          }
          break;
        }
        // Indirect jump - funky.
        case Inst::Kind::JI: {
          // Nothing to do here - transfers control to an already visited
          // function, without any data dependencies.
          break;
        }
        // Load - generate read constraint.
        case Inst::Kind::LD: {
          auto &loadInst = static_cast<LoadInst &>(inst);
          if (auto *addr = ctx.Lookup(loadInst.GetAddr())) {
            ctx.Map(loadInst, solver_.Load(addr));
          }
          break;
        }
        // Store - generate write constraint.
        case Inst::Kind::ST: {
          auto &storeInst = static_cast<StoreInst &>(inst);
          if (auto *value = ctx.Lookup(storeInst.GetVal())) {
            if (auto *addr = ctx.Lookup(storeInst.GetAddr())) {
              solver_.Store(addr, value);
            }
          }
          break;
        }
        // Exchange - generate read and write constraint.
        case Inst::Kind::XCHG: {
          auto &xchgInst = static_cast<ExchangeInst &>(inst);
          auto *addr = ctx.Lookup(xchgInst.GetAddr());
          if (auto *value = ctx.Lookup(xchgInst.GetVal())) {
            solver_.Store(addr, value);
          }
          ctx.Map(xchgInst, solver_.Load(addr));
          break;
        }
        // Register set - extra funky.
        case Inst::Kind::SET: {
          // Nothing to do here - restores the stack, however it does not
          // introduce any new data dependencies.
          break;
        }
        // Returns the current function's vararg state.
        case Inst::Kind::VASTART: {
          auto &vaStartInst = static_cast<VAStartInst &>(inst);
          if (auto *value = ctx.Lookup(vaStartInst.GetVAList())) {
            solver_.Subset(funcSet.VA, value);
          }
          break;
        }
        // Pointers to the stack frame.
        case Inst::Kind::FRAME: {
          auto &frameInst = static_cast<FrameInst &>(inst);
          const unsigned obj = frameInst.GetObject();
          RootNode *node;
          if (auto it = funcSet.Frame.find(obj); it != funcSet.Frame.end()) {
            node = it->second;
          } else {
            node = solver_.Root(solver_.Root());
            funcSet.Frame.insert({ obj, node });
          }
          ctx.Map(inst, node);
          break;
        }
        case Inst::Kind::ALLOCA: {
          ctx.Map(inst, funcSet.Alloca);
          break;
        }

        // Unary instructions - propagate pointers.
        case Inst::Kind::ABS:
        case Inst::Kind::NEG:
        case Inst::Kind::SQRT:
        case Inst::Kind::SEXT:
        case Inst::Kind::ZEXT:
        case Inst::Kind::FEXT:
        case Inst::Kind::TRUNC: {
          auto &unaryInst = static_cast<UnaryInst &>(inst);
          if (auto *arg = ctx.Lookup(unaryInst.GetArg())) {
            ctx.Map(unaryInst, arg);
          }
          break;
        }

        // Binary instructions - union of pointers.
        case Inst::Kind::ADD:
        case Inst::Kind::SUB:
        case Inst::Kind::AND:
        case Inst::Kind::OR:
        case Inst::Kind::ROTL:
        case Inst::Kind::SLL:
        case Inst::Kind::SRA:
        case Inst::Kind::SRL:
        case Inst::Kind::XOR:
        case Inst::Kind::CMP:
        case Inst::Kind::UADDO:
        case Inst::Kind::UMULO: {
          auto &binaryInst = static_cast<BinaryInst &>(inst);
          auto *lhs = ctx.Lookup(binaryInst.GetLHS());
          auto *rhs = ctx.Lookup(binaryInst.GetRHS());
          if (auto *c = solver_.Union(lhs, rhs)) {
            ctx.Map(binaryInst, c);
          }
          break;
        }

        // Select - union of return values.
        case Inst::Kind::SELECT: {
          auto &selectInst = static_cast<SelectInst &>(inst);
          auto *vt = ctx.Lookup(selectInst.GetTrue());
          auto *vf = ctx.Lookup(selectInst.GetFalse());
          if (auto *c = solver_.Union(vt, vf)) {
            ctx.Map(selectInst, c);
          }
          break;
        }

        // PHI - create an empty set.
        case Inst::Kind::PHI: {
          ctx.Map(inst, solver_.Empty());
          break;
        }

        // Mov - introduce symbols.
        case Inst::Kind::MOV: {
          if (auto *c = BuildValue(ctx, static_cast<MovInst &>(inst).GetArg())) {
            ctx.Map(inst, c);
          }
          break;
        }

        // Arg - tie to arg constraint.
        case Inst::Kind::ARG: {
          auto &argInst = static_cast<ArgInst &>(inst);
          unsigned idx = argInst.GetIdx();
          if (idx < funcSet.Args.size()) {
            ctx.Map(argInst, funcSet.Args[idx]);
          } else {
            llvm::report_fatal_error(
                "Argument " + std::to_string(idx) + " out of range in " +
                std::string(func->GetName())
            );
          }
          break;
        }

        // Undefined - +-inf.
        case Inst::Kind::UNDEF: {
          break;
        }

        // Instructions which do not produce pointers - ignored.
        case Inst::Kind::SIN:
        case Inst::Kind::COS:
        case Inst::Kind::EXP:
        case Inst::Kind::EXP2:
        case Inst::Kind::LOG:
        case Inst::Kind::LOG2:
        case Inst::Kind::LOG10:
        case Inst::Kind::FCEIL:
        case Inst::Kind::FFLOOR:
        case Inst::Kind::POPCNT:
        case Inst::Kind::CLZ:
        case Inst::Kind::DIV:
        case Inst::Kind::REM:
        case Inst::Kind::MUL:
        case Inst::Kind::POW:
        case Inst::Kind::COPYSIGN:
        case Inst::Kind::RDTSC: {
          break;
        }

        // Control flow - ignored.
        case Inst::Kind::JCC:
        case Inst::Kind::JMP:
        case Inst::Kind::SWITCH:
        case Inst::Kind::TRAP: {
          break;
        }
      }
    }
  }

  for (auto &block : *func) {
    for (auto &phi : block.phis()) {
      std::vector<Node *> ins;
      for (unsigned i = 0; i < phi.GetNumIncoming(); ++i) {
        if (auto *c = BuildValue(ctx, phi.GetValue(i))) {
          if (std::find(ins.begin(), ins.end(), c) == ins.end()) {
            ins.push_back(c);
          }
        }
      }

      auto *pc = ctx.Lookup(&phi);
      for (auto *c : ins) {
        solver_.Subset(c, pc);
      }
    }
  }
}

// -----------------------------------------------------------------------------
PTAContext::FunctionContext &PTAContext::BuildFunction(
    const std::vector<Inst *> &calls,
    Func *func)
{
  auto key = func;
  auto it = funcs_.emplace(key, nullptr);
  if (it.second) {
    it.first->second = std::make_unique<FunctionContext>();
    auto f = it.first->second.get();
    f->Return = solver_.Root();
    f->VA = solver_.Root();
    f->Alloca = solver_.Root(solver_.Root());
    for (auto &arg : func->params()) {
      f->Args.push_back(solver_.Root());
    }
    f->Expanded = false;
  }
  return *it.first->second;
}

// -----------------------------------------------------------------------------
Node *PTAContext::BuildValue(LocalContext &ctx, Value *v)
{
  switch (v->GetKind()) {
    case Value::Kind::INST: {
      // Instruction - propagate.
      return ctx.Lookup(static_cast<Inst *>(v));
    }
    case Value::Kind::GLOBAL: {
      // Global - set with global.
      return solver_.Lookup(static_cast<Global *>(v));
    }
    case Value::Kind::EXPR: {
      // Expression - set with offset.
      switch (static_cast<Expr *>(v)->GetKind()) {
        case Expr::Kind::SYMBOL_OFFSET: {
          auto *symExpr = static_cast<SymbolOffsetExpr *>(v);
          return solver_.Lookup(symExpr->GetSymbol());
        }
      }
    }
    case Value::Kind::CONST: {
      // Constant value - no constraint.
      return nullptr;
    }
  }
  llvm_unreachable("invalid value kind");
};


// -----------------------------------------------------------------------------
template<typename T>
Node *PTAContext::BuildCall(
    const std::vector<Inst *> &calls,
    LocalContext &ctx,
    Inst *caller,
    Inst *callee,
    llvm::iterator_range<typename CallSite<T>::arg_iterator> &&args)
{
  std::vector<Inst *> callString(calls);
  callString.push_back(caller);

  if (auto *global = ToGlobal(callee)) {
    if (auto *fn = ::dyn_cast_or_null<Func>(global)) {
      if (auto *c = BuildAlloc<T>(ctx, calls, fn->GetName(), args)) {
        // If the function is an allocation site, stop and
        // record it. Otherwise, recursively traverse callees.
        explored_.insert(fn);
        return c;
      } else {
        auto &funcSet = BuildFunction(callString, fn);
        unsigned i = 0;
        for (auto *arg : args) {
          if (auto *c = ctx.Lookup(arg)) {
            if (i >= funcSet.Args.size()) {
              if (fn->IsVarArg()) {
                solver_.Subset(c, funcSet.VA);
              }
            } else {
              solver_.Subset(c, funcSet.Args[i]);
            }
          }
          ++i;
        }
        queue_.emplace_back(callString, fn);
        return funcSet.Return;
      }
    }
    if (auto *ext = ::dyn_cast_or_null<Extern>(global)) {
      if (ext->getName() == "pthread_create") {
        // Pthread_create is just like an indirect call - first two args are
        // subsets of extern, 3rd is the target and the 4th is the argument.
        llvm::SmallVector<Inst *, 4> vec(args.begin(), args.end());
        assert(vec.size() == 4 && "invalid number of args to pthread");
        auto *externs = solver_.External();

        if (auto *thread = ctx.Lookup(vec[0])) {
          solver_.Subset(thread, externs);
        }
        if (auto *attr = ctx.Lookup(vec[1])) {
          solver_.Subset(attr, externs);
        }

        auto *ret = solver_.Root();
        calls_.emplace_back(
            callString,
            solver_.Anchor(ctx.Lookup(vec[2])),
            std::vector<RootNode *>{
              solver_.Anchor(ctx.Lookup(vec[3]))
            },
            ret
        );

        return ret;
      } else if (auto *c = BuildAlloc<T>(ctx, calls, ext->GetName(), args)) {
        return c;
      } else {
        auto *externs = solver_.External();
        for (auto *arg : args) {
          if (auto *c = ctx.Lookup(arg)) {
            solver_.Subset(c, externs);
          }
        }
        return externs;
      }
    }
    llvm::report_fatal_error("Attempting to call invalid global");
  } else {
    // Indirect call - constraint to be expanded later.
    std::vector<RootNode *> argsRoot;
    for (auto *arg : args) {
      argsRoot.push_back(solver_.Anchor(ctx.Lookup(arg)));
    }

    auto *ret = solver_.Root();
    calls_.emplace_back(
        callString,
        solver_.Anchor(ctx.Lookup(callee)),
        argsRoot,
        ret
    );
    return ret;
  }
};

// -----------------------------------------------------------------------------
template<typename T>
Node *PTAContext::BuildAlloc(
    LocalContext &ctx,
    const std::vector<Inst *> &calls,
    const std::string_view &name,
    llvm::iterator_range<typename CallSite<T>::arg_iterator> &args)
{
  static const char *allocs[] = {
    "caml_alloc1",
    "caml_alloc2",
    "caml_alloc3",
    "caml_allocN",
    "caml_alloc_young",
    "caml_fl_allocate",
    "caml_stat_alloc_noexc",
    "malloc",
  };
  static const char *reallocs[] = {
    "realloc",
    "caml_stat_resize_noexc"
  };

  for (size_t i = 0; i < sizeof(allocs) / sizeof(allocs[0]); ++i) {
    if (allocs[i] == name) {
      return solver_.Alloc(calls);
    }
  }

  for (size_t i = 0; i <sizeof(reallocs) / sizeof(reallocs[0]); ++i) {
    if (allocs[i] == name) {
      return ctx.Lookup(*args.begin());
    }
  }

  return nullptr;
};

// -----------------------------------------------------------------------------
std::optional<int64_t> PTAContext::ToInteger(Inst *inst)
{
  if (auto *movInst = ::dyn_cast_or_null<MovInst>(inst)) {
    if (auto *intConst = ::dyn_cast_or_null<ConstantInt>(movInst->GetArg())) {
      if (intConst->GetValue().getMinSignedBits() >= 64) {
        return intConst->GetInt();
      }
    }
  }
  return std::nullopt;
}

// -----------------------------------------------------------------------------
Global *PTAContext::ToGlobal(Inst *inst)
{
  if (auto *movInst = ::dyn_cast_or_null<MovInst>(inst)) {
    if (auto *global = ::dyn_cast_or_null<Global>(movInst->GetArg())) {
      return global;
    }
  }
  return nullptr;
}

// -----------------------------------------------------------------------------
std::vector<std::pair<std::vector<Inst *>, Func *>> PTAContext::Expand()
{
  std::vector<std::pair<std::vector<Inst *>, Func *>> callees;
  for (auto &call : calls_) {
    for (auto id : call.Callee->Set()->points_to_func()) {
      auto *func = solver_.Map(id);

      // Expand each call site only once.
      if (!call.Expanded.insert(func).second) {
        continue;
      }

      // Call to be expanded, with context.
      callees.emplace_back(call.Context, func);

      // Connect arguments and return value.
      auto &funcSet = BuildFunction(call.Context, func);
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
      solver_.Subset(funcSet.Return, call.Return);
    }

    for (auto id : call.Callee->Set()->points_to_ext()) {
      assert(!"not implemented");
    }
  }

  // Look at the extern set - call all funcs which reach it.
  auto *external = solver_.External();
  for (auto id : external->Set()->points_to_func()) {
    auto *func = solver_.Map(id);

    // Expand each call site only once.
    if (!externCallees_.insert(func).second) {
      continue;
    }

    // Call to be expanded, with context.
    callees.emplace_back(std::vector<Inst *>{}, func);

    // Connect arguments and return value.
    auto &funcSet = BuildFunction({}, func);
    for (unsigned i = 0; i< func->params().size(); ++i) {
      solver_.Subset(external, funcSet.Args[i]);
    }
    solver_.Subset(funcSet.Return, external);
  }

  return callees;
}

// -----------------------------------------------------------------------------
const char *PointsToAnalysis::kPassID = "pta";

// -----------------------------------------------------------------------------
void PointsToAnalysis::Run(Prog *prog)
{
  PTAContext graph(prog);

  for (auto &func : *prog) {
    if (func.GetVisibility() == Visibility::EXTERN) {
      graph.Explore(&func);
    }
  }

  for (auto &func : *prog) {
    if (graph.Reachable(&func)) {
      reachable_.insert(&func);
    }
  }
}

// -----------------------------------------------------------------------------
const char *PointsToAnalysis::GetPassName() const
{
  return "Points-To Analysis";
}

// -----------------------------------------------------------------------------
char AnalysisID<PointsToAnalysis>::ID;
