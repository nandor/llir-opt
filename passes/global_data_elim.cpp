// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <memory>
#include <unordered_map>
#include <unordered_set>

#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/ADT/SmallPtrSet.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/constant.h"
#include "core/data.h"
#include "core/dominator.h"
#include "core/func.h"
#include "core/insts.h"
#include "core/prog.h"
#include "passes/global_data_elim.h"

#include "global_data_elim/bag.h"
#include "global_data_elim/constraint.h"
#include "global_data_elim/heap.h"
#include "global_data_elim/solver.h"



/**
 * Global context, building and solving constraints.
 */
class GlobalContext final {
public:
  /// Initialises the context, scanning globals.
  GlobalContext(Prog *prog);

  /// Explores the call graph starting from a function.
  void Explore(Func *func)
  {
    queue_.push_back(func);
    while (!queue_.empty()) {
      while (!queue_.empty()) {
        Func *func = queue_.back();
        queue_.pop_back();
        BuildConstraints(func);
        solver.Progress();
      }
      for (auto &func : solver.Expand()) {
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
  /// Context for a function - mapping instructions to constraints.
  class LocalContext {
  public:
    /// Adds a new mapping.
    void Map(Inst &inst, Constraint *c)
    {
      if (c) {
        values_[&inst] = c;
      }
    }

    /// Finds a constraint for an instruction.
    Constraint *Lookup(Inst *inst)
    {
      return values_[inst];
    }

  private:
    /// Mapping from instructions to constraints.
    std::unordered_map<Inst *, Constraint *> values_;
  };

  /// Builds constraints for a single function.
  void BuildConstraints(Func *func);
  /// Builds a constraint for a single global.
  Constraint *BuildGlobal(Global *g);
  // Builds a constraint from a value.
  Constraint *BuildValue(LocalContext &ctx, Value *v);
  // Creates a constraint for a call.
  template<typename T>
  Constraint *BuildCall(
      LocalContext &ctx,
      Inst *caller,
      Inst *callee,
      llvm::iterator_range<typename CallSite<T>::arg_iterator> &&args
  );
  // Creates a constraint for a potential allocation site.
  template<typename T>
  Constraint *BuildAlloc(
      LocalContext &ctx,
      const std::string_view &name,
      llvm::iterator_range<typename CallSite<T>::arg_iterator> &args
  );

  /// Extracts an integer from a potential mov instruction.
  std::optional<int> ToInteger(Inst *inst);
  /// Extracts a global from a potential mov instruction.
  Global *ToGlobal(Inst *inst);

private:
  /// Set of explored constraints.
  ConstraintSolver solver;
  /// Work queue for functions to explore.
  std::vector<Func *> queue_;
  /// Set of explored functions.
  std::unordered_set<Func *> explored_;
  /// Offsets of atoms.
  std::unordered_map<Atom *, std::pair<DataNode *, unsigned>> offsets_;
};


// -----------------------------------------------------------------------------
GlobalContext::GlobalContext(Prog *prog)
{
  std::vector<std::tuple<Atom *, DataNode *, unsigned>> fixups;

  unsigned offset = 0;
  DataNode *chunk = nullptr;
  for (auto *data : prog->data()) {
    for (auto &atom : *data) {
      chunk = chunk ? chunk : solver.Node<DataNode>(&atom);
      offsets_[&atom] = std::make_pair(chunk, offset);

      for (auto *item : atom) {
        switch (item->GetKind()) {
          case Item::Kind::INT8:    offset += 1; break;
          case Item::Kind::INT16:   offset += 2; break;
          case Item::Kind::INT32:   offset += 4; break;
          case Item::Kind::INT64:   offset += 8; break;
          case Item::Kind::FLOAT64: offset += 8; break;
          case Item::Kind::SPACE:   offset += item->GetSpace(); break;
          case Item::Kind::STRING:  offset += item->GetString().size(); break;
          case Item::Kind::SYMBOL: {
            auto *global = item->GetSymbol();
            switch (global->GetKind()) {
              case Global::Kind::SYMBOL: {
                assert(!"not implemented");
                break;
              }
              case Global::Kind::EXTERN: {
                auto *ext = static_cast<Extern *>(global);
                chunk->Store(offset, Bag::Item(ext));
                break;
              }
              case Global::Kind::FUNC: {
                auto *func = static_cast<Func *>(global);
                chunk->Store(offset, Bag::Item(func));
                break;
              }
              case Global::Kind::BLOCK: {
                assert(!"not implemented");
                break;
              }
              case Global::Kind::ATOM: {
                fixups.emplace_back(static_cast<Atom *>(global), chunk, offset);
                break;
              }
            }
            offset += 8;
            break;
          }
          case Item::Kind::ALIGN: {
            auto mask = (1 << item->GetAlign()) - 1;
            offset = (offset + mask) & ~mask;
            break;
          }
          case Item::Kind::END: {
            offset = 0;
            chunk = nullptr;
            break;
          }
        }
      }
    }
  }

  for (auto &fixup : fixups) {
    auto [atom, chunk, offset] = fixup;
    auto [ptrChunk, ptrOff] = offsets_[atom];
    chunk->Store(offset, Bag::Item(ptrChunk, ptrOff));
  }
}

// -----------------------------------------------------------------------------
void GlobalContext::BuildConstraints(Func *func)
{
  if (!explored_.insert(func).second) {
    return;
  }

  // Some banned functions...
  if (func->getName() == "caml_alloc_for_heap") {
    return;
  }
  if (func->getName() == "caml_empty_minor_heap") {
    return;
  }
  if (func->getName() ==  "caml_stat_free") {
    return;
  }
  if (func->getName() == "caml_sys_exit") {
    return;
  }
  if (func->getName() == "caml_gc_dispatch") {
    return;
  }
  if (func->getName() == "caml_page_table_add") {
    return;
  }
  if (func->getName() == "caml_page_table_add") {
    return;
  }
  if (func->getName() == "caml_page_table_modify.22") {
    return;
  }
  if (func->getName() == "caml_insert_global_root.8") {
    return;
  }
  if (func->getName() == "caml_make_free_blocks") {
    return;
  }
  if (func->getName() == "caml_fl_merge_block") {
    return;
  }
  if (func->getName() == "caml_fl_init_merge") {
    return;
  }
  if (func->getName() == "caml_modify"){
    return;
  }
  if (func->getName() == "caml_page_table_lookup") {
    return;
  }

  llvm::errs() << func->getName() << "\n";

  // Constraint sets for the function.
  auto &funcSet = solver[func];
  LocalContext ctx;

  // For each instruction, generate a constraint.
  for (auto *block : llvm::ReversePostOrderTraversal<Func*>(func)) {
    for (auto &inst : *block) {
      switch (inst.GetKind()) {
        // Call - explore.
        case Inst::Kind::CALL: {
          auto &call = static_cast<CallInst &>(inst);
          auto *callee = call.GetCallee();
          if (auto *c = BuildCall<ControlInst>(ctx, &inst, callee, call.args())) {
            ctx.Map(call, c);
          }
          break;
        }
        // Invoke Call - explore.
        case Inst::Kind::INVOKE: {
          auto &call = static_cast<InvokeInst &>(inst);
          auto *callee = call.GetCallee();
          if (auto *c = BuildCall<TerminatorInst>(ctx, &inst, callee, call.args())) {
            ctx.Map(call, c);
          }
          break;
        }
        // Tail Call - explore.
        case Inst::Kind::TCALL:
        case Inst::Kind::TINVOKE: {
          auto &call = static_cast<CallSite<TerminatorInst>&>(inst);
          auto *callee = call.GetCallee();
          if (auto *c = BuildCall<TerminatorInst>(ctx, &inst, callee, call.args())) {
            solver.Subset(c, funcSet.Return);
          }
          break;
        }
        // Return - generate return constraint.
        case Inst::Kind::RET: {
          auto &retInst = static_cast<ReturnInst &>(inst);
          if (auto *val = retInst.GetValue()) {
            if (auto *c = ctx.Lookup(val)) {
              solver.Subset(c, funcSet.Return);
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
          if (*loadInst.GetSize() == 8) {
            ctx.Map(loadInst, solver.Load(ctx.Lookup(loadInst.GetAddr())));
          }
          break;
        }
        // Store - generate write constraint.
        case Inst::Kind::ST: {
          auto &storeInst = static_cast<StoreInst &>(inst);
          auto *storeVal = storeInst.GetVal();
          if (auto *value = ctx.Lookup(storeVal)) {
            if (*storeInst.GetSize() == 8) {
              solver.Store(ctx.Lookup(storeInst.GetAddr()), value);
            }
          }
          break;
        }
        // Exchange - generate read and write constraint.
        case Inst::Kind::XCHG: {
          auto &xchgInst = static_cast<ExchangeInst &>(inst);
          auto *addr = ctx.Lookup(xchgInst.GetAddr());
          if (auto *value = ctx.Lookup(xchgInst.GetVal())) {
            solver.Store(addr, value);
          }
          ctx.Map(xchgInst, solver.Load(addr));
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
          ctx.Map(inst, funcSet.VA);
          break;
        }
        // Returns an offset into the functions's frame.
        case Inst::Kind::FRAME: {
          ctx.Map(inst, funcSet.Frame);
          break;
        }

        // Unary instructions - propagate pointers.
        case Inst::Kind::ABS:
        case Inst::Kind::NEG:
        case Inst::Kind::SQRT:
        case Inst::Kind::SIN:
        case Inst::Kind::COS:
        case Inst::Kind::SEXT:
        case Inst::Kind::ZEXT:
        case Inst::Kind::FEXT:
        case Inst::Kind::TRUNC: {
          break;
        }

        // Compute offsets.
        case Inst::Kind::ADD:
        case Inst::Kind::SUB: {
          auto &addInst = static_cast<BinaryInst &>(inst);
          int64_t sign = inst.GetKind() == Inst::Kind::SUB ? -1 : +1;
          auto *lhs = ctx.Lookup(addInst.GetLHS());
          auto *rhs = ctx.Lookup(addInst.GetRHS());

          if (lhs && rhs) {
            ctx.Map(addInst, solver.Union(
                solver.Offset(lhs),
                solver.Offset(rhs)
            ));
          } else if (lhs) {
            if (auto c = ToInteger(addInst.GetRHS())) {
              ctx.Map(addInst, solver.Offset(lhs, sign * *c));
            } else {
              ctx.Map(addInst, solver.Offset(lhs));
            }
          } else if (rhs) {
            if (auto c = ToInteger(addInst.GetLHS())) {
              ctx.Map(addInst, solver.Offset(rhs, sign * *c));
            } else {
              ctx.Map(addInst, solver.Offset(rhs));
            }
          }
          break;
        }

        // Binary instructions - union of pointers.
        case Inst::Kind::AND:
        case Inst::Kind::OR:
        case Inst::Kind::ROTL:
        case Inst::Kind::SLL:
        case Inst::Kind::SRA:
        case Inst::Kind::SRL:
        case Inst::Kind::XOR: {
          auto &binaryInst = static_cast<BinaryInst &>(inst);
          auto *lhs = ctx.Lookup(binaryInst.GetLHS());
          auto *rhs = ctx.Lookup(binaryInst.GetRHS());
          if (auto *c = solver.Union(lhs, rhs)) {
            ctx.Map(binaryInst, c);
          }
          break;
        }

        // Binary instructions - don't propagate pointers.
        case Inst::Kind::CMP:
        case Inst::Kind::DIV:
        case Inst::Kind::REM:
        case Inst::Kind::MUL:
        case Inst::Kind::POW:
        case Inst::Kind::COPYSIGN:
        case Inst::Kind::UADDO:
        case Inst::Kind::UMULO: {
          break;
        }

        // Select - union of all.
        case Inst::Kind::SELECT: {
          auto &selectInst = static_cast<SelectInst &>(inst);
          auto *cond = ctx.Lookup(selectInst.GetCond());
          auto *vt = ctx.Lookup(selectInst.GetTrue());
          auto *vf = ctx.Lookup(selectInst.GetFalse());
          if (auto *c = solver.Union(cond, vt, vf)) {
            ctx.Map(selectInst, c);
          }
          break;
        }

        // PHI - create an empty set.
        case Inst::Kind::PHI: {
          ctx.Map(inst, solver.Ptr(solver.Bag(), false));
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
          ctx.Map(argInst, funcSet.Args[argInst.GetIdx()]);
          break;
        }

        // Undefined - +-inf.
        case Inst::Kind::UNDEF: {
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
      std::vector<Constraint *> ins;
      for (unsigned i = 0; i < phi.GetNumIncoming(); ++i) {
        if (auto *c = BuildValue(ctx, phi.GetValue(i))) {
          if (std::find(ins.begin(), ins.end(), c) != ins.end()) {
            ins.push_back(c);
          }
        }
      }

      auto *pc = ctx.Lookup(&phi);
      for (auto *c : ins) {
        solver.Subset(c, pc);
      }
    }
  }
}

// -----------------------------------------------------------------------------
Constraint *GlobalContext::BuildGlobal(Global *g)
{
  switch (g->GetKind()) {
    case Global::Kind::SYMBOL: {
      return nullptr;
    }
    case Global::Kind::EXTERN: {
      return solver.Ptr(solver.Bag(static_cast<Extern *>(g)), true);
    }
    case Global::Kind::FUNC: {
      return solver.Ptr(solver.Bag(static_cast<Func *>(g)), true);
    }
    case Global::Kind::BLOCK: {
      return nullptr;
    }
    case Global::Kind::ATOM: {
      auto [chunk, off] = offsets_[static_cast<Atom *>(g)];
      return solver.Ptr(solver.Bag(chunk, off), true);
    }
  }
}

// -----------------------------------------------------------------------------
Constraint *GlobalContext::BuildValue(LocalContext &ctx, Value *v)
{
  switch (v->GetKind()) {
    case Value::Kind::INST: {
      // Instruction - propagate.
      return ctx.Lookup(static_cast<Inst *>(v));
    }
    case Value::Kind::GLOBAL: {
      // Global - set with global.
      return BuildGlobal(static_cast<Global *>(v));
    }
    case Value::Kind::EXPR: {
      // Expression - set with offset.
      switch (static_cast<Expr *>(v)->GetKind()) {
        case Expr::Kind::SYMBOL_OFFSET: {
          auto *symExpr = static_cast<SymbolOffsetExpr *>(v);
          return solver.Offset(
              BuildGlobal(symExpr->GetSymbol()),
              symExpr->GetOffset()
          );
        }
      }
    }
    case Value::Kind::CONST: {
      // Constant value - no constraint.
      return nullptr;
    }
  }
};


// -----------------------------------------------------------------------------
template<typename T>
Constraint *GlobalContext::BuildCall(
    LocalContext &ctx,
    Inst *caller,
    Inst *callee,
    llvm::iterator_range<typename CallSite<T>::arg_iterator> &&args)
{
  if (auto *global = ToGlobal(callee)) {
    if (auto *calleeFunc = ::dyn_cast_or_null<Func>(global)) {
      // If the function is an allocation site, stop and
      // record it. Otherwise, recursively traverse callees.
      if (auto *c = BuildAlloc<T>(ctx, calleeFunc->GetName(), args)) {
        return c;
      } else {
        auto &funcSet = solver[calleeFunc];
        unsigned i = 0;
        for (auto *arg : args) {
          if (auto *c = ctx.Lookup(arg)) {
            if (i >= funcSet.Args.size()) {
              if (calleeFunc->IsVarArg()) {
                solver.Subset(c, funcSet.VA);
              }
            } else {
              solver.Subset(c, funcSet.Args[i]);
            }
          }
          ++i;
        }
        queue_.push_back(calleeFunc);
        return funcSet.Return;
      }
    }
    if (auto *ext = ::dyn_cast_or_null<Extern>(global)) {
      if (auto *c = BuildAlloc<T>(ctx, ext->GetName(), args)) {
        return c;
      } else {
        auto *externs = solver.Extern();
        for (auto *arg : args) {
          if (auto *c = ctx.Lookup(arg)) {
            solver.Subset(c, externs);
          }
        }
        return solver.Offset(externs);
      }
    }
    throw std::runtime_error("Attempting to call invalid global");
  } else {
    std::vector<Constraint *> argConstraint;
    for (auto *arg : args) {
      argConstraint.push_back(ctx.Lookup(arg));
    }
    return solver.Call(caller, ctx.Lookup(callee), argConstraint);
  }
};

// -----------------------------------------------------------------------------
template<typename T>
Constraint *GlobalContext::BuildAlloc(
    LocalContext &ctx,
    const std::string_view &name,
    llvm::iterator_range<typename CallSite<T>::arg_iterator> &args)
{
  auto AllocSize = [&, this]() {
    return ToInteger(*args.begin()).value_or(0);
  };

  if (name == "caml_alloc1") {
    return solver.Ptr(solver.Bag(solver.Node<CamlNode>(1), 0), false);
  }
  if (name == "caml_alloc2") {
    return solver.Ptr(solver.Bag(solver.Node<CamlNode>(2), 0), false);
  }
  if (name == "caml_alloc3") {
    return solver.Ptr(solver.Bag(solver.Node<CamlNode>(3), 0), false);
  }
  if (name == "caml_allocN") {
    return solver.Ptr(solver.Bag(solver.Node<CamlNode>(AllocSize() / 8), 0), false);
  }
  if (name == "caml_alloc") {
    return solver.Ptr(solver.Bag(solver.Node<SetNode>(), 0), false);
  }
  if (name == "caml_alloc_small") {
    return solver.Ptr(solver.Bag(solver.Node<SetNode>(), 0), false);
  }
  if (name == "caml_fl_allocate") {
    return solver.Ptr(solver.Bag(solver.Node<SetNode>(), 0), false);
  }
  if (name == "caml_stat_alloc_noexc") {
    return solver.Ptr(solver.Bag(solver.Node<SetNode>(), 0), false);
  }
  if (name == "caml_alloc_shr_aux.22") {
    return solver.Ptr(solver.Bag(solver.Node<SetNode>(), 0), false);
  }
  if (name == "caml_stat_alloc") {
    return solver.Ptr(solver.Bag(solver.Node<SetNode>(), 0), false);
  }
  if (name == "caml_alloc_custom") {
    return solver.Ptr(solver.Bag(solver.Node<SetNode>(), 0), false);
  }
  if (name == "malloc") {
    return solver.Ptr(solver.Bag(solver.Node<SetNode>(), 0), false);
  }
  if (name == "realloc") {
    return ctx.Lookup(*args.begin());
  }
  if (name == "caml_stat_resize_noexc") {
    return ctx.Lookup(*args.begin());
  }
  return nullptr;
};

// -----------------------------------------------------------------------------
std::optional<int> GlobalContext::ToInteger(Inst *inst)
{
  if (auto *movInst = ::dyn_cast_or_null<MovInst>(inst)) {
    if (auto *intConst = ::dyn_cast_or_null<ConstantInt>(movInst->GetArg())) {
      return intConst->GetValue();
    }
  }
  return std::nullopt;
}

// -----------------------------------------------------------------------------
Global *GlobalContext::ToGlobal(Inst *inst)
{
  if (auto *movInst = ::dyn_cast_or_null<MovInst>(inst)) {
    if (auto *global = ::dyn_cast_or_null<Global>(movInst->GetArg())) {
      return global;
    }
  }
  return nullptr;
}

// -----------------------------------------------------------------------------
void GlobalDataElimPass::Run(Prog *prog)
{
  GlobalContext graph(prog);

  if (auto *main = ::dyn_cast_or_null<Func>(prog->GetGlobal("main"))) {
    graph.Explore(main);
  }
  /*
  if (auto *gc = ::dyn_cast_or_null<Func>(prog->GetGlobal("caml_garbage_collection"))) {
    graph.Explore(gc);
  }
  */

  std::vector<Func *> funcs;
  for (auto it = prog->begin(); it != prog->end(); ) {
    auto *func = &*it++;
    if (graph.Reachable(func)) {
      continue;
    }

    Func *undef = nullptr;
    for (auto ut = func->use_begin(); ut != func->use_end(); ) {
      Use &use = *ut++;
      if (use.getUser() == nullptr) {
        if (!undef) {
          undef = new Func(prog, std::string(func->getName()) + "$undef");
          auto *block = new Block(undef, "entry");
          undef->AddBlock(block);
          auto *inst = new TrapInst();
          block->AddInst(inst);
          funcs.push_back(undef);
        }
        use = undef;
      }
    }
  }
  for (auto *f : funcs) {
    prog->AddFunc(f);
  }
}

// -----------------------------------------------------------------------------
const char *GlobalDataElimPass::GetPassName() const
{
  return "Global Data Elimination Pass";
}
