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

class ConstraintSolver;



/**
 * An item storing a constraint.
 */
class Constraint {
public:
  enum class Kind {
    SET,
    SUBSET,
    UNION,
    OFFSET,
    LOAD,
    CALL
  };

  Constraint(Kind kind)
    : kind_(kind)
  {
  }

  /// Returns the node kind.
  Kind GetKind() const { return kind_; }

protected:
  class Use {
  public:
    /// Creates a new reference to a value.
    Use(Constraint *user, Constraint *value)
      : User(user)
      , Value(value)
      , Next(nullptr)
      , Prev(nullptr)
    {
    }

    /// Returns the used value.
    operator Constraint * () const { return Value; }

  private:
    /// User constraint.
    Constraint *User;
    /// Used value.
    Constraint *Value;
    /// Next item in the use chain.
    Constraint *Next;
    /// Previous item in the use chain.
    Constraint *Prev;
  };

private:
  /// The solver should access all fields.
  friend class ConstraintSolver;

  /// Kind of the node.
  Kind kind_;
  /// List of users.
  Constraint *users_;
  /// Previous node in the chain of all constraints.
  Constraint *prev_;
  /// Next node in the chain of all nodes.
  Constraint *next_;
};

/**
 * Base class of nodes modelling the heap.
 */
class Node {
public:
  Node() { }
};

/**
 * Simple node, used to represent C allocation points.
 */
class SimpleNode final : public Node {
public:
};

/**
 * Node representing items in a data segment.
 */
class DataNode final : public Node {
public:
  DataNode()
  {
  }

public:
  /// Each field of the global chunk is modelled independently.
  std::map<unsigned, Constraint *> Fields;
};

/**
 * Node representing an OCaml allocation point.
 */
class CamlNode final : public Node {
public:
  CamlNode(unsigned size)
  {
  }

};



// -----------------------------------------------------------------------------
class CSet final : public Constraint {
public:
  CSet()
    : Constraint(Kind::SET)
  {
  }

  CSet(Node *node, unsigned off)
    : Constraint(Kind::SET)
  {
  }

  CSet(Func *func)
    : Constraint(Kind::SET)
  {
  }

  CSet(Extern *ext)
    : Constraint(Kind::SET)
  {
  }

private:

};

class CSubset final : public Constraint {
public:
  /// Creates a subset constraint.
  CSubset(Constraint *subset, Constraint *set)
    : Constraint(Kind::SUBSET)
    , subset_(this, subset)
    , set_(this, set)
  {
  }

  /// Returns the subset.
  Constraint *GetSubset() const { return subset_; }
  /// Returns the set.
  Constraint *GetSet() const { return set_; }

private:
  /// Subset.
  Constraint::Use subset_;
  /// Set.
  Constraint::Use set_;
};

class CUnion final : public Constraint {
public:
  /// Creates a union constraint.
  CUnion(Constraint *lhs, Constraint *rhs)
    : Constraint(Kind::UNION)
    , lhs_(this, lhs)
    , rhs_(this, rhs)
  {
  }

  /// Returns the LHS of the union.
  Constraint *GetLHS() const { return lhs_; }
  /// Returns the RHS of the union.
  Constraint *GetRHS() const { return rhs_; }

private:
  /// LHS of the union.
  Constraint::Use lhs_;
  /// RHS of the union.
  Constraint::Use rhs_;
};

class COffset final : public Constraint {
public:
  /// Creates a new offset node with infinite offset.
  COffset(Constraint *ptr)
    : Constraint(Kind::OFFSET)
    , ptr_(this, ptr)
  {
  }

  /// Creates a new offset node with no offset.
  COffset(Constraint *ptr, int64_t off)
    : Constraint(Kind::OFFSET)
    , ptr_(this, ptr)
    , off_(off)
  {
  }

  /// Returns a pointer.
  Constraint *GetPointer() const { return ptr_; }
  /// Returns the offset.
  std::optional<int64_t> GetOffset() const { return off_; }

private:
  /// Dereferenced pointer.
  Constraint::Use ptr_;
  /// Offset (if there is one).
  std::optional<int64_t> off_;
};

class CLoad final : public Constraint {
public:
  /// Creates a new load constraint.
  CLoad(Constraint *ptr)
    : Constraint(Kind::LOAD)
    , ptr_(this, ptr)
  {
  }

  /// Returns the pointer.
  Constraint *GetPointer() { return ptr_; }

private:
  /// Dereferenced pointer.
  Constraint::Use ptr_;
};

class CCall final : public Constraint {
public:
  /// Creates a new call constraint.
  CCall(Constraint *callee, std::vector<Constraint *> &args)
    : Constraint(Kind::CALL)
    , nargs_(args.size())
    , callee_(this, callee)
    , args_(static_cast<Constraint::Use *>(malloc(sizeof(Constraint::Use) * nargs_)))
  {
    for (unsigned i = 0; i < args.size(); ++i) {
      new (&args_[i]) Constraint::Use(this, args[i]);
    }
  }

  /// Returns the callee.
  Constraint *GetCallee() const { return callee_; }
  /// Returns the number of arguments.
  unsigned GetNumArgs() const { return nargs_; }
  /// Returns the ith argument.
  Constraint *GetArg(unsigned i) { return args_[i]; }

private:
  /// Number of args.
  unsigned nargs_;
  /// Callee.
  Constraint::Use callee_;
  /// Arguments.
  Constraint::Use *args_;
};



// -----------------------------------------------------------------------------
class ConstraintSolver final {
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
  ConstraintSolver()
    : head_(nullptr)
    , tail_(nullptr)
    , extern_(Set())
  {
  }

  /// Creates a store constraint.
  void Store(Constraint *ptr, Constraint *val)
  {
    Subset(val, Load(ptr));
  }

  /// Returns a load constraint.
  Constraint *Load(Constraint *ptr)
  {
    return Make<CLoad>(ptr);
  }

  /// Generates a subset constraint.
  void Subset(Constraint *a, Constraint *b)
  {
    Make<CSubset>(a, b);
  }

  /// Generates a new, empty set constraint.
  Constraint *Set()
  {
    return Make<CSet>();
  }

  /// Generates a new node with a single pointer.
  Constraint *Set(Node *node)
  {
    return Make<CSet>(node, 0);
  }

  /// Generates a set pointing to a single extern.
  Constraint *Set(Extern *ext)
  {
    return Make<CSet>(ext);
  }

  /// Generates a set pointing to a single function.
  Constraint *Set(Func *func)
  {
    return Make<CSet>(func);
  }

  /// Generates a set pointing to a single global.
  Constraint *Set(DataNode *chunk, unsigned offset)
  {
    return Make<CSet>(chunk, offset);
  }

  /// Creates an offset constraint, +-inf.
  Constraint *Offset(Constraint *c)
  {
    return Make<COffset>(c);
  }

  /// Creates an offset constraint.
  Constraint *Offset(Constraint *c, int64_t offset)
  {
    return Make<COffset>(c, offset);
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
    return Make<CUnion>(a, b);
  }

  /// Returns a ternary set union.
  Constraint *Union(Constraint *a, Constraint *b, Constraint *c)
  {
    return Union(a, Union(b, c));
  }

  /// Indirect call, to be expanded.
  Constraint *Call(Constraint *callee, std::vector<Constraint *> args)
  {
    return Make<CCall>(callee, args);
  }

  /// Extern function context.
  Constraint *Extern()
  {
    return extern_;
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

  void Dump()
  {
    for (auto *node = head_; node; node = node->next_) {
      switch (node->GetKind()) {
        case Constraint::Kind::SET: {
          llvm::errs() << node << " = set(";
          llvm::errs() << ")\n";
          break;
        }
        case Constraint::Kind::SUBSET: {
          auto *csubset = static_cast<CSubset *>(node);
          llvm::errs() << "subset(";
          llvm::errs() << csubset->GetSubset();
          llvm::errs() << ", ";
          llvm::errs() << csubset->GetSet();
          llvm::errs() << ")\n";
          break;
        }
        case Constraint::Kind::UNION: {
          auto *cunion = static_cast<CUnion *>(node);
          llvm::errs() << node << " = union(";
          llvm::errs() << cunion->GetLHS();
          llvm::errs() << ", ";
          llvm::errs() << cunion->GetRHS();
          llvm::errs() << ")\n";
          break;
        }
        case Constraint::Kind::OFFSET: {
          auto *coffset = static_cast<COffset *>(node);
          llvm::errs() << node << " = offset(";
          llvm::errs() << coffset->GetPointer() << ", ";
          if (auto off = coffset->GetOffset()) {
            llvm::errs() << *off;
          } else {
            llvm::errs() << "inf";
          }
          llvm::errs() << ")\n";
          break;
        }
        case Constraint::Kind::LOAD: {
          auto *cload = static_cast<CLoad *>(node);
          llvm::errs() << node << " = load(";
          llvm::errs() << cload->GetPointer();
          llvm::errs() << ")\n";
          break;
        }
        case Constraint::Kind::CALL: {
          auto *ccall = static_cast<CCall *>(node);
          llvm::errs() << node << " = call(";
          llvm::errs() << ccall->GetCallee();
          for (unsigned i = 0; i < ccall->GetNumArgs(); ++i) {
            llvm::errs() << ", " << ccall->GetArg(i);
          }
          llvm::errs() << ")\n";
          break;
        }
      }
    }
  }

private:
  /// Constructs a node.
  template<typename T, typename ...Args>
  T *Make(Args... args) {
    T *node = new T(args...);
    if (!head_) {
      head_ = tail_ = node;
      node->prev_ = nullptr;
      node->next_ = nullptr;
    } else {
      node->next_ = nullptr;
      node->prev_ = tail_;
      tail_->next_ = node;
      tail_ = node;
    }
    return node;
  }

private:
  /// Function argument/return constraints.
  std::unordered_map<Func *, std::unique_ptr<FuncSet>> funcs_;
  /// Head of the node list.
  Constraint *head_;
  /// Tail of the node list.
  Constraint *tail_;
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

    solver.Dump();
  }

private:
  /// Builds constraints for a single function.
  void BuildConstraints(Func *func);

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
  DataNode *chunk;
  for (auto *data : prog->data()) {
    for (auto &atom : *data) {
      chunk = chunk ? chunk : new DataNode();
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
                chunk->Fields.emplace(offset, solver.Set(ext));
                break;
              }
              case Global::Kind::FUNC: {
                auto *func = static_cast<Func *>(global);
                chunk->Fields.emplace(offset, solver.Set(func));
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

  auto BuildGlobal = [&, this] (Global *g) -> Constraint * {
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
  };

  auto BuildCamlNode = [&, this] (unsigned n) -> Node * {
    if (n % 8 == 0) {
      return new CamlNode(n / 8);
    } else {
      assert(!"not implemented");
    }
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

  // Creates a constraint for a potential allocation site.
  auto BuildAlloc = [&, this](auto &name, const auto &args) -> Constraint * {
    auto AllocSize = [&, this]() {
      return ValInteger(*args.begin()).value_or(0);
    };

    if (name == "caml_alloc1") {
      return solver.Set(BuildCamlNode(8));
    }
    if (name == "caml_alloc2") {
      return solver.Set(BuildCamlNode(16));
    }
    if (name == "caml_alloc3") {
      return solver.Set(BuildCamlNode(24));
    }
    if (name == "caml_allocN") {
      return solver.Set(BuildCamlNode(AllocSize()));
    }
    if (name == "caml_alloc") {
      return solver.Set(new SimpleNode());
    }
    if (name == "caml_alloc_small") {
      return solver.Set(new SimpleNode());
    }
    if (name == "caml_fl_allocate") {
      return solver.Set(new SimpleNode());
    }
    if (name == "malloc") {
      return solver.Set(new SimpleNode());
    }
    if (name == "realloc") {
      return Lookup(*args.begin());
    }
    return nullptr;
  };

  // Creates a constraint for a call.
  auto BuildCall = [&, this](Inst *callee, auto &&args) -> Constraint * {
    if (auto *global = ValGlobal(callee)) {
      if (auto *calleeFunc = ::dyn_cast_or_null<Func>(global)) {
        // If the function is an allocation site, stop and
        // record it. Otherwise, recursively traverse callees.
        if (auto *c = BuildAlloc(calleeFunc->GetName(), args)) {
          return c;
        } else {
          auto &funcSet = solver[calleeFunc];
          unsigned i = 0;
          for (auto *arg : args) {
            if (auto *c = Lookup(arg)) {
              if (i >= funcSet.Args.size() && calleeFunc->IsVarArg()) {
                solver.Subset(c, funcSet.VA);
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
        if (auto *c = BuildAlloc(ext->GetName(), args)) {
          return c;
        } else {
          auto *externs = solver.Extern();
          for (auto *arg : args) {
            if (auto *c = Lookup(arg)) {
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
      //printer.Print(&inst);
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
          if (auto *value = Lookup(storeInst.GetVal())) {
            solver.Store(Lookup(storeInst.GetAddr()), value);
          }
          break;
        }
        // Exchange - generate read and write constraint.
        case Inst::Kind::XCHG: {
          auto &xchgInst = static_cast<ExchangeInst &>(inst);
          auto *addr = Lookup(xchgInst.GetAddr());
          if (auto *value = Lookup(xchgInst.GetVal())) {
            solver.Store(addr, value);
          }
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
            Map(addInst, solver.Union(
                solver.Offset(lhs),
                solver.Offset(rhs)
            ));
          } else if (lhs) {
            if (auto c = ValInteger(addInst.GetRHS())) {
              Map(addInst, solver.Offset(lhs, *c));
            } else {
              Map(addInst, solver.Offset(lhs));
            }
          } else if (rhs) {
            if (auto c = ValInteger(addInst.GetLHS())) {
              Map(addInst, solver.Offset(rhs, *c));
            } else {
              Map(addInst, solver.Offset(rhs));
            }
          }
          break;
        }

        // Binary instructions - union of pointers.
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
