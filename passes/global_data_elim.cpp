// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <unordered_map>
#include <unordered_set>
#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/ADT/SmallPtrSet.h>
#include "core/block.h"
#include "core/constant.h"
#include "core/cast.h"
#include "core/data.h"
#include "core/dominator.h"
#include "core/func.h"
#include "core/prog.h"
#include "core/insts.h"
#include "passes/global_data_elim.h"



// -----------------------------------------------------------------------------
class Constraint final {
public:
  enum class Kind {
    OFFSET,
    READ,
    WRITE,
    SET,
    ELEM,
    SUBSET,
    CALL
  };
};


// -----------------------------------------------------------------------------
class Segment {
public:
  /// Each field of the global chunk is modelled independently.
  std::map<unsigned, Constraint *> Fields;
};


// -----------------------------------------------------------------------------
class ConstraintSet final {
public:
  struct FuncSet {
    /// Argument sets.
    std::vector<Constraint *> Args;
    /// Return set.
    Constraint *Return;
    /// Frame of the function.
    Constraint *Frame;
    /// Variable argument glob.
    Constraint *VA;
  };

public:
  ConstraintSet()
    : extern_(Set())
  {
  }

  /// Generates a subset constraint.
  void Subset(Constraint *a, Constraint *b)
  {
  }

  /// Generates a new, empty set constraint.
  Constraint *Set()
  {
    return nullptr;
  }

  /// Generates a set pointing to a single extern.
  Constraint *Set(Extern *ext)
  {
    return nullptr;
  }

  /// Generates a set pointing to a single function.
  Constraint *Set(Func *func)
  {
    return nullptr;
  }

  /// Generates a set pointing to a single global.
  Constraint *Set(Segment *chunk, unsigned offset)
  {
    return nullptr;
  }

  /// Creates a store constraint.
  void Store(Constraint *ptr, Constraint *val)
  {

  }

  /// Returns a load constraint.
  Constraint *Load(Constraint *ptr)
  {
    return nullptr;
  }

  /// Creates an offset constraint.
  Constraint *Offset(Constraint *c, int64_t offset)
  {
    return nullptr;
  }

  /// Returns the constraints attached to a function.
  FuncSet &operator[](Func *func)
  {
    auto it = funcs_.emplace(func, nullptr);
    if (it.second) {
      it.first->second = std::make_unique<FuncSet>();
      auto f = it.first->second.get();
      f->Return = Set();
      f->VA = Set();
      f->Frame = Set();
      for (auto &arg : func->params()) {
        f->Args.push_back(Set());
      }
    }
    return *it.first->second;
  }

  /// Returns a binary set union.
  Constraint *Union(Constraint *a, Constraint *b)
  {
    if (!a) {
      return b;
    }
    if (!b) {
      return a;
    }
    assert(!"not implemented");
  }

  /// Returns a ternary set union.
  Constraint *Union(Constraint *a, Constraint *b, Constraint *c)
  {
    return Union(a, Union(b, c));
  }

  /// Indirect call, to be expanded.
  Constraint *Call(Constraint *callee, std::vector<Constraint *> args)
  {
    return nullptr;
  }

  /// Extern function context.
  Constraint *Extern()
  {
    return extern_;
  }

private:
  /// Function argument/return constraints.
  std::unordered_map<Func *, std::unique_ptr<FuncSet>> funcs_;
  /// Bag for external values.
  Constraint *extern_;
};


// -----------------------------------------------------------------------------
class GlobalContext final {
public:
  /// Initialises the context, scanning globals.
  GlobalContext(Prog *prog);

  /// Explores the call graph starting from a function.
  void Explore(Func *func)
  {
    queue_.push_back(func);
    while (!queue_.empty()) {
      Func *func = queue_.back();
      queue_.pop_back();
      BuildConstraints(func);
    }
  }

private:
  /// Builds constraints for a global.
  Constraint *BuildGlobal(Global *g);
  /// Builds constraints for a single function.
  void BuildConstraints(Func *func);

private:
  /// Set of explored constraints.
  ConstraintSet solver;
  /// Work queue for functions to explore.
  std::vector<Func *> queue_;
  /// Set of explored functions.
  std::unordered_set<Func *> explored_;
  /// Offsets of atoms.
  std::unordered_map<Atom *, std::pair<Segment *, unsigned>> offsets_;
};

// -----------------------------------------------------------------------------
GlobalContext::GlobalContext(Prog *prog)
{
  std::vector<std::tuple<Atom *, Segment *, unsigned>> fixups;

  unsigned offset = 0;
  Segment *chunk;
  for (auto *data : prog->data()) {
    for (auto &atom : *data) {
      chunk = chunk ? chunk : new Segment();
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
              case Global::Kind::SYMBOL: assert(!"not implemented"); break;
              case Global::Kind::EXTERN: assert(!"not implemented"); break;
              case Global::Kind::FUNC: {
                auto *func = static_cast<Func *>(global);
                chunk->Fields.emplace(offset, solver.Set(func));
                break;
              }
              case Global::Kind::BLOCK: assert(!"not implemented"); break;
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
      return solver.Set(static_cast<Extern *>(g));
    }
    case Global::Kind::FUNC: {
      return solver.Set(static_cast<Func *>(g));
    }
    case Global::Kind::BLOCK: {
      return nullptr;
    }
    case Global::Kind::ATOM: {
      auto [chunk, off] = offsets_[static_cast<Atom *>(g)];
      return solver.Set(chunk, off);
    }
  }
}

// -----------------------------------------------------------------------------
void GlobalContext::BuildConstraints(Func *func)
{
  if (!explored_.insert(func).second) {
    return;
  }

  // Maps a value to a constraint.
  std::unordered_map<Inst *, Constraint *> values;
  auto Map = [&values](Inst &inst, Constraint *c) {
    if (c) {
      values[&inst] = c;
    }
  };
  auto Lookup = [&values](Inst *inst) -> Constraint * {
    return values[inst];
  };

  // Checks if an argument is a constant.
  auto ValInteger = [](Inst *inst) -> std::optional<int> {
    if (auto *movInst = ::dyn_cast_or_null<MovInst>(inst)) {
      if (auto *intConst = ::dyn_cast_or_null<ConstantInt>(movInst->GetArg())) {
        return intConst->GetValue();
      }
    }
    return std::nullopt;
  };

  // Checks if the argument is a function.
  auto ValGlobal = [](Inst *inst) -> Global * {
    if (auto *movInst = ::dyn_cast_or_null<MovInst>(inst)) {
      if (auto *global = ::dyn_cast_or_null<Global>(movInst->GetArg())) {
        return global;
      }
    }
    return nullptr;
  };

  // Builds a constraint from a value.
  auto ValConstraint = [&, this](Value *v) -> Constraint * {
    switch (v->GetKind()) {
      case Value::Kind::INST: {
        // Instruction - propagate.
        return Lookup(static_cast<Inst *>(v));
      }
      case Value::Kind::GLOBAL: {
        return BuildGlobal(static_cast<Global *>(v));
      }
      case Value::Kind::EXPR: {
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

  // Creates a constraint for a call.
  auto BuildCall = [&, this](Inst *callee, auto &&args) -> Constraint * {
    if (auto *global = ValGlobal(callee)) {
      if (auto *calleeFunc = ::dyn_cast_or_null<Func>(global)) {
        auto &funcSet = solver[calleeFunc];
        unsigned i = 0;
        for (auto *arg : args) {
          solver.Subset(Lookup(arg), funcSet.Args[i]);
          ++i;
        }
        queue_.push_back(calleeFunc);
        return funcSet.Return;
      }
      if (auto *ext = ::dyn_cast_or_null<Extern>(global)) {
        auto *externs = solver.Extern();
        for (auto *arg : args) {
          solver.Subset(Lookup(arg), externs);
        }
        return externs;
      }
      throw std::runtime_error("Attempting to call invalid global");
    } else {
      std::vector<Constraint *> argConstraint;
      for (auto *arg : args) {
        argConstraint.push_back(Lookup(arg));
      }
      return solver.Call(Lookup(callee), argConstraint);
    }
  };

  // Constraint sets for the function.
  auto &funcSet = solver[func];

  // For each instruction, generate a constraint.
  for (auto *block : llvm::ReversePostOrderTraversal<Func*>(func)) {
    for (auto &inst : *block) {
      switch (inst.GetKind()) {
        // Call - explore.
        case Inst::Kind::CALL: {
          auto &callInst = static_cast<CallInst &>(inst);
          if (auto *c = BuildCall(callInst.GetCallee(), callInst.args())) {
            Map(callInst, c);
          }
          break;
        }
        // Invoke Call - explore.
        case Inst::Kind::INVOKE: {
          auto &invokeInst = static_cast<InvokeInst &>(inst);
          if (auto *c = BuildCall(invokeInst.GetCallee(), invokeInst.args())) {
            Map(invokeInst, c);
          }
          break;
        }
        // Tail Call - explore.
        case Inst::Kind::TCALL:
        case Inst::Kind::TINVOKE: {
          auto &termInst = static_cast<CallSite<TerminatorInst>&>(inst);
          if (auto *c = BuildCall(termInst.GetCallee(), termInst.args())) {
            solver.Subset(c, funcSet.Return);
          }
          break;
        }
        // Return - generate return constraint.
        case Inst::Kind::RET: {
          if (auto *c = Lookup(&inst)) {
            solver.Subset(c, funcSet.Return);
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
          Map(loadInst, solver.Load(Lookup(loadInst.GetAddr())));
          break;
        }
        // Store - generate read constraint.
        case Inst::Kind::ST: {
          auto &storeInst = static_cast<StoreInst &>(inst);
          solver.Store(Lookup(storeInst.GetAddr()), Lookup(storeInst.GetVal()));
          break;
        }
        // Exchange - generate read and write constraint.
        case Inst::Kind::XCHG: {
          auto &xchgInst = static_cast<ExchangeInst &>(inst);
          auto *addr = Lookup(xchgInst.GetAddr());
          auto *val = Lookup(xchgInst.GetVal());
          solver.Store(addr, val);
          Map(xchgInst, solver.Load(addr));
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
          Map(inst, funcSet.VA);
          break;
        }
        // Returns an offset into the functions's frame.
        case Inst::Kind::FRAME: {
          Map(inst, funcSet.Frame);
          break;
        }

        // Unary instructions - introduce +-inf.
        case Inst::Kind::ABS:
        case Inst::Kind::NEG:
        case Inst::Kind::SQRT:
        case Inst::Kind::SIN:
        case Inst::Kind::COS:
        case Inst::Kind::SEXT:
        case Inst::Kind::ZEXT:
        case Inst::Kind::FEXT:
        case Inst::Kind::TRUNC: {
          auto &unaryInst = static_cast<UnaryInst &>(inst);
          if (auto *arg = Lookup(unaryInst.GetArg())) {
            Map(unaryInst, arg);
          }
          break;
        }

        // Compute offsets.
        case Inst::Kind::ADD:
        case Inst::Kind::SUB: {
          auto &addInst = static_cast<BinaryInst &>(inst);
          auto *lhs = Lookup(addInst.GetLHS());
          auto *rhs = Lookup(addInst.GetRHS());

          if (lhs && rhs) {
            assert(!"not implemented");
          } else if (auto c = ValInteger(addInst.GetLHS())) {
            if (rhs) {
              assert(!"not implemented");
            }
          } else if (auto c = ValInteger(addInst.GetRHS())) {
            if (lhs) {
              assert(!"not implemented");
            }
          }
          break;
        }

        // Binary instructions - introduce +-inf.
        case Inst::Kind::AND:
        case Inst::Kind::CMP:
        case Inst::Kind::DIV:
        case Inst::Kind::REM:
        case Inst::Kind::MUL:
        case Inst::Kind::OR:
        case Inst::Kind::ROTL:
        case Inst::Kind::SLL:
        case Inst::Kind::SRA:
        case Inst::Kind::SRL:
        case Inst::Kind::XOR:
        case Inst::Kind::POW:
        case Inst::Kind::COPYSIGN:
        case Inst::Kind::UADDO:
        case Inst::Kind::UMULO: {
          auto &binaryInst = static_cast<BinaryInst &>(inst);
          auto *lhs = Lookup(binaryInst.GetLHS());
          auto *rhs = Lookup(binaryInst.GetRHS());
          if (auto *c = solver.Union(lhs, rhs)) {
            Map(binaryInst, c);
          }
          break;
        }

        // Select - union of all.
        case Inst::Kind::SELECT: {
          auto &selectInst = static_cast<SelectInst &>(inst);
          auto *cond = Lookup(selectInst.GetCond());
          auto *vt = Lookup(selectInst.GetTrue());
          auto *vf = Lookup(selectInst.GetFalse());
          if (auto *c = solver.Union(cond, vt, vf)) {
            Map(selectInst, c);
          }
          break;
        }

        // PHI - create an empty set.
        case Inst::Kind::PHI: {
          Map(inst, solver.Set());
          break;
        }

        // Mov - introduce symbols.
        case Inst::Kind::MOV: {
          if (auto *c = ValConstraint(static_cast<MovInst &>(inst).GetArg())) {
            Map(inst, c);
          }
          break;
        }

        // Arg - tie to arg constraint.
        case Inst::Kind::ARG: {
          auto &argInst = static_cast<ArgInst &>(inst);
          Map(argInst, funcSet.Args[argInst.GetIdx()]);
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
      for (unsigned i = 0; i < phi.GetNumIncoming(); ++i) {
        if (auto *c = ValConstraint(phi.GetValue(i))) {
          solver.Subset(c, Lookup(&phi));
        }
      }
    }
  }
}

// -----------------------------------------------------------------------------
void GlobalDataElimPass::Run(Prog *prog)
{
  GlobalContext graph(prog);

  if (auto *main = ::dyn_cast_or_null<Func>(prog->GetGlobal("main"))) {
    graph.Explore(main);
  }
}

// -----------------------------------------------------------------------------
const char *GlobalDataElimPass::GetPassName() const
{
  return "Global Data Elimination Pass";
}
