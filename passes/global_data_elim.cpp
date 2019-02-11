// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <memory>
#include <unordered_map>
#include <unordered_set>

#include <llvm/ADT/ilist.h>
#include <llvm/ADT/ilist_node.h>
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

class ConstraintSolver;
class Node;



/**
 * An item storing a constraint.
 */
class Constraint : public llvm::ilist_node<Constraint> {
protected:
  /**
   * Class to track uses of a constraint.
   */
  class Use {
  public:
    /// Creates a new reference to a value.
    Use(Constraint *user, Constraint *value)
      : user_(user)
      , value_(value)
    {
      if (value_) {
        next_ = value_->users_;
        prev_ = nullptr;
        if (next_) { next_->prev_ = this; }
        value_->users_ = this;
      } else {
        next_ = nullptr;
        prev_ = nullptr;
      }
    }

    /// Removes the value from the use chain.
    ~Use()
    {
      if (value_) {
        if (next_) { next_->prev_ = prev_; }
        if (prev_) { prev_->next_ = next_; }
        if (this == value_->users_) { value_->users_ = next_; }
        next_ = prev_ = nullptr;
      }
    }

    /// Returns the used value.
    operator Constraint * () const { return value_; }

    /// Returns the next use.
    Use *GetNext() const { return next_; }
    /// Returns the user.
    Constraint *GetUser() const { return user_; }

  private:
    friend class Constraint;
    /// User constraint.
    Constraint *user_;
    /// Used value.
    Constraint *value_;
    /// next_ item in the use chain.
    Use *next_;
    /// Previous item in the use chain.
    Use *prev_;
  };

  /**
   * Iterator over users.
   */
  template <typename UserTy>
  class iter_impl : public std::iterator<std::forward_iterator_tag, UserTy *> {
  public:
    // Preincrement
    iter_impl &operator++()
    {
      use_ = use_->GetNext();
      return *this;
    }

    // Postincrement
    iter_impl operator++(int)
    {
      auto tmp = *this;
      ++*this;
      return tmp;
    }

    // Retrieve a pointer to the current User.
    UserTy *operator*() const { return use_->GetUser(); }
    UserTy *operator->() const { return &operator*(); }

    // Iterator comparison.
    bool operator==(const iter_impl &x) const { return use_ == x.use_; }
    bool operator!=(const iter_impl &x) const { return use_ != x.use_; }

  private:
    friend class Constraint;

    iter_impl(Use *use) : use_(use) {}
    iter_impl() : use_(nullptr) {}

    Use *use_;
  };

  using iter = iter_impl<Constraint>;
  using const_iter = iter_impl<const Constraint>;

public:
  /// Enumeration of constraint kinds.
  enum class Kind {
    PTR,
    SUBSET,
    UNION,
    OFFSET,
    LOAD,
    STORE,
    CALL
  };

  /// Creates a new constraint.
  Constraint(Kind kind)
    : kind_(kind)
    , users_(nullptr)
  {
  }

  /// Deletes the constraint, remove it from use chains.
  virtual ~Constraint()
  {
    for (Use *use = users_; use; use = use->next_) {
      use->value_ = nullptr;
    }
  }

  /// Returns the node kind.
  Kind GetKind() const { return kind_; }
  /// Checks if the node is of a specific type.
  bool Is(Kind kind) { return GetKind() == kind; }

  // Iterator over users.
  bool empty() const { return users_ == nullptr; }
  iter begin() { return iter(users_); }
  const_iter begin() const { return const_iter(users_); }
  iter end() { return iter(); }
  const_iter end() const { return const_iter(); }
  llvm::iterator_range<iter> users() { return { begin(), end() }; }
  llvm::iterator_range<const_iter> users() const { return { begin(), end() }; }

private:
  /// The solver should access all fields.
  friend class ConstraintSolver;

  /// Kind of the node.
  Kind kind_;
  /// List of users.
  Use *users_;
};

/**
 * Bag of possible nodes.
 */
class Bag {
public:
  /// Represents an item to load from in a bag.
  class Item {
  private:
    enum class Kind {
      FUNC,
      EXT,
      NODE
    };

  public:
    Item(Node *node)
      : kind_(Kind::NODE)
      , nodeVal_(node)
    {
    }

    Item(Node *node, unsigned off)
      : kind_(Kind::NODE)
      , nodeVal_(node)
      , off_(off)
    {
    }

    Item(Extern *ext)
      : kind_(Kind::EXT)
      , extVal_(ext)
    {
    }

    Item(Func *func)
      : kind_(Kind::FUNC)
      , funcVal_(func)
    {
    }

    Func *GetFunc() const
    {
      return kind_ == Kind::FUNC ? funcVal_ : nullptr;
    }

    Extern *GetExtern() const
    {
      return kind_ == Kind::EXT ? extVal_ : nullptr;
    }

    using NodeTy = std::pair<Node *, std::optional<unsigned>>;
    std::optional<NodeTy> GetNode() const
    {
      if (kind_ == Kind::NODE) {
        return std::optional<NodeTy>{ NodeTy{ nodeVal_, off_ } };
      } else {
        return std::nullopt;
      }
    }

    /// Dereferences the item.
    void Load(std::function<void(Item&)> &&f) const;

    /// Offsets an item.
    std::optional<Item> Offset(const std::optional<int64_t> &off);

    /// Updates the item.
    bool Store(const Item &item);

  private:
    /// Bag should read everything.
    friend class Bag;

    /// Kind of the item.
    Kind kind_;

    /// Pointer to the item.
    union {
      Extern *extVal_;
      Func *funcVal_;
      Node *nodeVal_;
    };

    /// Offset into the item.
    std::optional<unsigned> off_;
  };

  /// Constructs an empty bag.
  Bag()
  {
  }

  /// Singleton node pointer.
  Bag(Node *node)
  {
    items_.emplace_back(node);
  }

  /// Singleton specific offset.
  Bag(Node *node, unsigned off)
  {
    items_.emplace_back(node, off);
  }

  /// Singleton external pointer.
  Bag(Extern *ext)
  {
    items_.emplace_back(ext);
  }

  /// Singleton function pointer.
  Bag(Func *func)
  {
    items_.emplace_back(func);
  }

  // Node iterators.
  using item_iter = std::vector<Item>::iterator;
  item_iter item_begin() { return items_.begin(); }
  item_iter item_end() { return items_.end(); }
  llvm::iterator_range<item_iter> items()
  {
    return llvm::make_range(item_begin(), item_end());
  }

  /// Stores an item into the bag.
  bool Store(const Item &item);

private:
  /// Stored items.
  std::vector<Item> items_;
};

/**
 * Base class of nodes modelling the heap.
 */
class Node {
public:
  Node() { }

  virtual void Load(std::function<void(Bag::Item&)> &&f) = 0;
  virtual void Load(unsigned off, std::function<void(Bag::Item&)> &&f) = 0;

  virtual std::optional<unsigned> GetSize() const = 0;

  virtual bool Store(const Bag::Item &item) = 0;
  virtual bool Store(unsigned off, const Bag::Item &item) = 0;
};

/**
 * Simple node, used to represent C allocation points.
 */
class SetNode final : public Node {
public:
  void Load(std::function<void(Bag::Item&)> &&f) override
  {
    for (auto &item : bag_.items()) {
      f(item);
    }
  }

  void Load(unsigned off, std::function<void(Bag::Item&)> &&f) override
  {
    return Load(std::forward<std::function<void(Bag::Item&)>>(f));
  }

  std::optional<unsigned> GetSize() const override
  {
    return std::nullopt;
  }

  bool Store(const Bag::Item &item) override
  {
    return bag_.Store(item);
  }

  bool Store(unsigned off, const Bag::Item &item) override
  {
    return Store(item);
  }

private:
  /// Underlying bag.
  Bag bag_;
};

/**
 * Node representing items in a data segment.
 */
class DataNode final : public Node {
public:
  DataNode(Atom *atom)
    : atom_(atom)
    , common_(nullptr)
  {
  }

  void Load(std::function<void(Bag::Item&)> &&f) override
  {
    for (auto &field : fields_) {
      for (auto &item : field.second->items()) {
        f(item);
      }
    }
    if (common_) {
      for (auto &item : common_->items()) {
        f(item);
      }
    }
  }

  void Load(unsigned off, std::function<void(Bag::Item&)> &&f) override
  {
    unsigned l = (off + 0) & ~7;
    unsigned h = (off + 8) & ~7;
    LoadSlot(l, std::forward<decltype(f)>(f));
    if (h != l) {
      LoadSlot(h, std::forward<decltype(f)>(f));
    }

    if (common_) {
      for (auto &item : common_->items()) {
        f(item);
      }
    }
  }

  std::optional<unsigned> GetSize() const override
  {
    return std::nullopt;
  }

  bool Store(const Bag::Item &item) override
  {
    if (!common_) {
      common_ = new Bag();
    }
    return common_->Store(item);
  }

  bool Store(unsigned off, const Bag::Item &item) override
  {
    unsigned l = (off + 0) & ~7;
    unsigned h = (off + 8) & ~7;
    bool changed = StoreSlot(l, item);
    if (h != l) {
      changed |= StoreSlot(h, item);
    }
    return changed;
  }

private:
  void LoadSlot(unsigned off, std::function<void(Bag::Item&)> &&f)
  {
    auto it_ = fields_.find(off);
    if (it_ == fields_.end()) {
      return;
    }

    for (auto &item : it_->second->items()) {
      f(item);
    }
  }

  bool StoreSlot(unsigned off, const Bag::Item &item)
  {
    auto it = fields_.emplace(off, nullptr);
    if (it.second) {
      it.first->second = new Bag();
    }

    auto *bag = it.first->second;
    return bag->Store(item);
  }

private:
  /// Source atom.
  Atom *atom_;
  /// Each field of the global chunk is modelled independently.
  std::map<unsigned, Bag *> fields_;
  /// Bag of items common to all fields.
  Bag *common_;
};

/**
 * Node representing an OCaml allocation point.
 */
class CamlNode final : public Node {
public:
  CamlNode(unsigned size)
    : size_(size)
  {
  }

  void Load(std::function<void(Bag::Item&)> &&f) override
  {
    if (header_) {
      assert(!"not implemented");
    }
    if (common_) {
      for (auto &item : common_->items()) {
        f(item);
      }
    }
    for (auto &field : fields_) {
      assert(!"not implemented");
    }
  }

  void Load(unsigned off, std::function<void(Bag::Item&)> &&f) override
  {
    assert(!"not implemented");
  }

  std::optional<unsigned> GetSize() const override
  {
    return size_ * 8;
  }

  bool Store(const Bag::Item &item) override
  {
    if (!common_) {
      common_ = std::make_unique<Bag>();
    }
    return common_->Store(item);
  }

  bool Store(unsigned off, const Bag::Item &item) override
  {
    assert(!"not implemented");
  }

private:
  unsigned size_;

  /// Values in the header.
  std::unique_ptr<Bag> header_;
  /// Values everywhere.
  std::unique_ptr<Bag> common_;
  /// Values stored in fields.
  std::vector<std::unique_ptr<Bag>> fields_;
};



// -----------------------------------------------------------------------------
class CPtr final : public Constraint, public Bag {
public:
  CPtr(Bag *bag, bool global)
    : Constraint(Kind::PTR)
    , bag_(bag)
    , global_(global)
  {
  }

  /// Returns a pointer.
  Bag *GetBag() const { return bag_; }
  /// Checks if the set is global.
  bool IsGlobal() const { return global_; }

private:
  /// Bag the pointer is pointing to.
  Bag *bag_;
  /// Flag indicating if the set is global.
  bool global_;
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
  Constraint *GetPointer() const { return ptr_; }

private:
  /// Dereferenced pointer.
  Constraint::Use ptr_;
};

class CStore final : public Constraint {
public:
  /// Creates a new store constraint.
  CStore(Constraint *val, Constraint *ptr)
    : Constraint(Kind::STORE)
    , val_(this, val)
    , ptr_(this, ptr)
  {
  }

  /// Returns the value.
  Constraint *GetValue() const { return val_; }
  /// Returns the pointer.
  Constraint *GetPointer() const { return ptr_; }

private:
  /// Value to write.
  Constraint::Use val_;
  /// Pointer written to.
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
  Constraint *GetArg(unsigned i) const { return args_[i]; }

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
    : extern_(Fix(Ptr(Bag(), true)))
  {
  }

  /// Creates a store constraint.
  Constraint *Store(Constraint *ptr, Constraint *val)
  {
    return Fix(Make<CStore>(val, ptr));
  }

  /// Returns a load constraint.
  Constraint *Load(Constraint *ptr)
  {
    return Make<CLoad>(ptr);
  }

  /// Generates a subset constraint.
  Constraint *Subset(Constraint *a, Constraint *b)
  {
    return Fix(Make<CSubset>(a, b));
  }

  /// Generates a new, empty set constraint.
  Constraint *Ptr(Bag *bag, bool global)
  {
    return Make<CPtr>(bag, global);
  }

  /// Generates a new, empty set constraint.
  template<typename ...Args>
  Bag *Bag(Args... args)
  {
    return new class Bag(args...);
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
    return Fix(Make<CCall>(callee, args));
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
      f->Return = Fix(Ptr(Bag(), true));
      f->VA = Fix(Ptr(Bag(), true));
      f->Frame = Fix(Ptr(Bag(), true));
      for (auto &arg : func->params()) {
        f->Args.push_back(Fix(Ptr(Bag(), true)));
      }
    }
    return *it.first->second;
  }

  /// Dumps the constraints to stdout.
  void Dump(const Constraint *c)
  {
    auto &os = llvm::errs();
    switch (c->GetKind()) {
      case Constraint::Kind::PTR: {
        auto *cptr = static_cast<const CPtr *>(c);
        auto *bag = cptr->GetBag();
        os << c << " = ptr{";
        bool needsComma = false;
        for (auto &item : bag->items()) {
          if (needsComma) os << ", "; needsComma = true;
          if (auto *func = item.GetFunc()) {
            os << func->getName();
          }
          if (auto *ext = item.GetExtern()) {
            os << ext->getName();
          }
          if (auto node = item.GetNode()) {
            os << node->first;
            if (node->second) {
              os << "+" << *node->second;
            } else {
              os << "+inf";
            }
          }
        }
        os << "}\n";
        break;
      }
      case Constraint::Kind::SUBSET: {
        auto *csubset = static_cast<const CSubset *>(c);
        os << "subset(";
        os << csubset->GetSubset() << ", " << csubset->GetSet();
        os << ")\n";
        break;
      }
      case Constraint::Kind::UNION: {
        auto *cunion = static_cast<const CUnion *>(c);
        os << c << " = union(";
        os << cunion->GetLHS() << ", " << cunion->GetRHS();
        os << ")\n";
        break;
      }
      case Constraint::Kind::OFFSET: {
        auto *coffset = static_cast<const COffset *>(c);
        os << c << " = offset(";
        os << coffset->GetPointer() << ", ";
        if (auto off = coffset->GetOffset()) {
          os << *off;
        } else {
          os << "inf";
        }
        os << ")\n";
        break;
      }
      case Constraint::Kind::LOAD: {
        auto *cload = static_cast<const CLoad *>(c);
        os << c << " = load(" << cload->GetPointer() << ")\n";
        break;
      }
      case Constraint::Kind::STORE: {
        auto *cstore = static_cast<const CStore *>(c);
        os << "store(";
        os << cstore->GetValue() << ", " << cstore->GetPointer();
        os << ")\n";
        break;
      }
      case Constraint::Kind::CALL: {
        auto *ccall = static_cast<const CCall *>(c);
        os << c << " = call(";
        os << ccall->GetCallee();
        for (unsigned i = 0; i < ccall->GetNumArgs(); ++i) {
          os << ", " << ccall->GetArg(i);
        }
        os << ")\n";
        break;
      }
    }
  }

  /// Simplifies the constraints.
  void Progress()
  {
    std::vector<Constraint *> queue;
    std::unordered_set<Constraint *> inQueue;

    // Remove the dangling nodes which were not fixed.
    for (auto &node : dangling_) {
      delete node;
    }
    dangling_.clear();

    // Find the root nodes to propagate values from.
    for (auto &node : batch_) {
      if (node.Is(Constraint::Kind::PTR)) {
        queue.push_back(&node);
        inQueue.insert(&node);
      }
    }

    // Propagate values from sets: this is guaranteed to converge.
    while (!queue.empty()) {
      Constraint *c = queue.back();
      queue.pop_back();
      inQueue.erase(c);

      bool propagate = false;

      // Evaluate the constraint, updating the rule node.
      switch (c->GetKind()) {
        case Constraint::Kind::PTR: {
          propagate = true;
          break;
        }
        case Constraint::Kind::SUBSET: {
          auto *csubset = static_cast<CSubset *>(c);
          auto *from = Lookup(csubset->GetSubset());
          auto *to = Lookup(csubset->GetSet());

          for (auto &item : from->items()) {
            propagate |= to->Store(item);
          }

          auto *set = csubset->GetSet();
          if (propagate && !inQueue.count(set)) {
            inQueue.insert(set);
            queue.push_back(set);
          }
          continue;
        }
        case Constraint::Kind::UNION: {
          auto *cunion = static_cast<CUnion *>(c);
          auto *lhs = Lookup(cunion->GetLHS());
          auto *rhs = Lookup(cunion->GetRHS());
          auto *to = Lookup(cunion);

          for (auto &item : lhs->items()) {
            propagate |= to->Store(item);
          }
          for (auto &item : rhs->items()) {
            propagate |= to->Store(item);
          }
          break;
        }
        case Constraint::Kind::OFFSET: {
          auto *coffset = static_cast<COffset *>(c);
          auto *from = Lookup(coffset->GetPointer());
          auto *to = Lookup(coffset);
          for (auto &item : from->items()) {
            if (auto newItem = item.Offset(coffset->GetOffset())) {
              propagate |= to->Store(*newItem);
            }
          }
          break;
        }
        case Constraint::Kind::LOAD: {
          auto *cload = static_cast<CLoad *>(c);
          auto *from = Lookup(cload->GetPointer());
          auto *to = Lookup(cload);

          for (auto &item : from->items()) {
            item.Load([&propagate, to](auto &item) {
              propagate |= to->Store(item);
            });
          }

          break;
        }
        case Constraint::Kind::STORE: {
          auto *cstore = static_cast<CStore *>(c);
          auto *from = Lookup(cstore->GetValue());
          auto *to = Lookup(cstore->GetPointer());

          for (auto &fromItem : from->items()) {
            fromItem.Load([&propagate, &fromItem, to](auto &item) {
              for (auto &toItem : to->items()) {
                propagate |= toItem.Store(fromItem);
              }
            });
          }
          continue;
        }
        case Constraint::Kind::CALL: {
          continue;
        }
      }

      // If the set of the node changed, propagate it forward to other nodes.
      if (propagate) {
        for (auto *user : c->users()) {
          if (inQueue.count(user)) {
            continue;
          }

          if (user->Is(Constraint::Kind::SUBSET)) {
            auto *csubset = static_cast<CSubset *>(user);
            if (csubset->GetSubset() != c) {
              continue;
            }
          }

          if (user->Is(Constraint::Kind::STORE)) {
            auto *cstore = static_cast<CStore *>(user);
            if (cstore->GetValue() != c) {
              continue;
            }
          }

          queue.push_back(user);
          inQueue.insert(user);
        }
      }
    }

    Simplify(batch_);

    fixed_.splice(fixed_.end(), batch_, batch_.begin(), batch_.end());
  }

  /// Remove irrelevant constraints.
  void Simplify(llvm::ilist<Constraint> &nodes)
  {
    std::vector<Constraint *> queue;
    std::unordered_set<Constraint *> keep;

    // Identify the important nodes - global sets and calls.
    for (auto &node : nodes) {
      if (node.Is(Constraint::Kind::PTR)) {
        if (static_cast<CPtr *>(&node)->IsGlobal()) {
          queue.push_back(&node);
          keep.insert(&node);
        }
        continue;
      }
      if (node.Is(Constraint::Kind::CALL)) {
        queue.push_back(&node);
        keep.insert(&node);
        continue;
      }
    }

    // Helper to queue a node.
    auto Visit = [&keep, &queue] (Constraint *node) {
      if (!keep.count(node)) {
        queue.push_back(node);
        keep.insert(node);
      }
    };

    // Tag their dependencies as live.
    while (!queue.empty()) {
      Constraint *c = queue.back();
      queue.pop_back();

      switch (c->GetKind()) {
        case Constraint::Kind::PTR: {
          // No used values to traverse.
          break;
        }
        case Constraint::Kind::STORE: {
          // If the store is live, its pointer is too.
          Visit(static_cast<CStore *>(c)->GetPointer());
          break;
        }
        case Constraint::Kind::SUBSET: {
          // If a subset is live, its source is too.
          Visit(static_cast<CSubset *>(c)->GetSet());
          break;
        }
        case Constraint::Kind::UNION: {
          // Visit both operands.
          Visit(static_cast<CUnion *>(c)->GetLHS());
          Visit(static_cast<CUnion *>(c)->GetRHS());
          break;
        }
        case Constraint::Kind::OFFSET: {
          // Offset operands are live.
          Visit(static_cast<COffset *>(c)->GetPointer());
          break;
        }
        case Constraint::Kind::LOAD: {
          // Load operands are live.
          Visit(static_cast<CLoad *>(c)->GetPointer());
          break;
        }
        case Constraint::Kind::CALL: {
          // Call operands are live.
          auto *ccall = static_cast<CCall *>(c);
          Visit(ccall->GetCallee());
          for (unsigned i = 0; i < ccall->GetNumArgs(); ++i) {
            if (auto *arg = ccall->GetArg(i)) {
              Visit(arg);
            }
          }
          break;
        }
      }

      /// Instruction which store stuff into an live node are live.
      for (auto *user : c->users()) {
        if (keep.count(user)) {
          continue;
        }

        if (user->Is(Constraint::Kind::SUBSET)) {
          if (static_cast<CSubset *>(user)->GetSet() == c) {
            keep.insert(user);
            queue.push_back(user);
          }
          continue;
        }

        if (user->Is(Constraint::Kind::STORE)) {
          if (static_cast<CStore *>(user)->GetPointer() == c) {
            keep.insert(user);
            queue.push_back(user);
          }
          continue;
        }
      }
    }

    // Remove all nodes which have not been visited.
    for (auto it = nodes.begin(); it != nodes.end(); ) {
      auto *c = &*it++;
      if (!keep.count(c)) {
        nodes.erase(c->getIterator());
      }
    }
  }

  /// Simplifies the whole batch.
  std::vector<Func *> Expand()
  {
    std::vector<Func *> callees;
    for (auto &node : fixed_) {
      if (!node.Is(Constraint::Kind::CALL)) {
        continue;
      }

      auto &call = static_cast<CCall &>(node);
      auto *bag = Lookup(call.GetCallee());
      for (auto &item : bag->items()) {
        if (auto *func = item.GetFunc()) {
          auto &expanded = expanded_[&call];
          llvm::errs() << func->getName() << "\n";
        }
      }
    }
    return callees;
  }

private:
  /// Constructs a node.
  template<typename T, typename ...Args>
  T *Make(Args... args)
  {
    T *node = new T(args...);
    dangling_.insert(node);
    return node;
  }

  /// Fixes a dangling reference.
  Constraint *Fix(Constraint *c)
  {
    auto it = dangling_.find(c);
    if (it == dangling_.end()) {
      return c;
    }

    dangling_.erase(it);

    switch (c->GetKind()) {
      case Constraint::Kind::PTR: {
        break;
      }
      case Constraint::Kind::SUBSET: {
        auto *csubset = static_cast<CSubset *>(c);
        Fix(csubset->GetSubset());
        Fix(csubset->GetSet());
        break;
      }
      case Constraint::Kind::UNION: {
        auto *cunion = static_cast<CUnion *>(c);
        Fix(cunion->GetLHS());
        Fix(cunion->GetRHS());
        break;
      }
      case Constraint::Kind::OFFSET: {
        auto *coffset = static_cast<COffset *>(c);
        Fix(coffset->GetPointer());
        break;
      }
      case Constraint::Kind::LOAD: {
        auto *cload = static_cast<CLoad *>(c);
        Fix(cload->GetPointer());
        break;
      }
      case Constraint::Kind::STORE: {
        auto *cstore = static_cast<CStore *>(c);
        Fix(cstore->GetValue());
        Fix(cstore->GetPointer());
        break;
      }
      case Constraint::Kind::CALL: {
        auto *ccall = static_cast<CCall *>(c);
        Fix(ccall->GetCallee());
        for (unsigned i = 0; i < ccall->GetNumArgs(); ++i) {
          Fix(ccall->GetArg(i));
        }
        break;
      }
    }

    batch_.push_back(c);

    return c;
  }

  /// Returns the bag for a value.
  class Bag *Lookup(Constraint *c)
  {
    if (c->Is(Constraint::Kind::PTR)) {
      return static_cast<CPtr *>(c)->GetBag();
    } else {
      auto it = bags_.emplace(c, nullptr);
      if (it.second) {
        it.first->second = new class Bag();
      }
      return it.first->second;
    }
  }

private:
  /// Function argument/return constraints.
  std::unordered_map<Func *, std::unique_ptr<FuncSet>> funcs_;
  /// List of fixed nodes.
  llvm::ilist<Constraint> batch_;
  /// New batch of nodes.
  llvm::ilist<Constraint> fixed_;
  /// Set of dangling nodes.
  std::unordered_set<Constraint *> dangling_;
  /// External bag.
  Constraint *extern_;
  /// Temp bags of some objects.
  std::unordered_map<Constraint *, class Bag *> bags_;
  /// Expanded callees for each call site.
  std::unordered_map<CCall *, std::set<Func *>> expanded_;
};

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
void Bag::Item::Load(std::function<void(Item&)> &&f) const
{
  switch (kind_) {
    case Kind::FUNC: {
      // Code of functions shouldn't be read.
      break;
    }
    case Kind::EXT: {
      // A bit iffy - might break things.
      break;
    }
    case Kind::NODE: {
      if (off_) {
        nodeVal_->Load(*off_, std::forward<std::function<void(Item&)>>(f));
      } else {
        nodeVal_->Load(std::forward<std::function<void(Item&)>>(f));
      }
      break;
    }
  }
}

// -----------------------------------------------------------------------------
bool Bag::Item::Store(const Item &item)
{
  switch (kind_) {
    case Kind::FUNC: {
      // Functions shouldn't be mutated.
      return false;
    }
    case Kind::EXT: {
      // External vars shouldn't be written.
      return false;
    }
    case Kind::NODE: {
      if (off_) {
        return nodeVal_->Store(*off_, item);
      } else {
        return nodeVal_->Store(item);
      }
    }
  }
}

// -----------------------------------------------------------------------------
std::optional<Bag::Item> Bag::Item::Offset(const std::optional<int64_t> &off)
{
  switch (kind_) {
    case Kind::FUNC: {
      return std::nullopt;
    }
    case Kind::EXT: {
      return std::nullopt;
    }
    case Kind::NODE: {
      if (auto size = nodeVal_->GetSize()) {
        if (off_ && off && *off_ + *off < size) {
          return std::optional<Item>(std::in_place, nodeVal_, *off_ + *off);
        }
      }
      return std::optional<Item>(std::in_place, nodeVal_);
    }
  }
}

// -----------------------------------------------------------------------------
bool Bag::Store(const Item &item)
{
  switch (item.kind_) {
    case Item::Kind::FUNC: {
      for (auto &other : items_) {
        if (other.kind_ != Item::Kind::FUNC) {
          continue;
        }
        if (other.funcVal_ == item.funcVal_) {
          return false;
        }
      }
      items_.push_back(item);
      return true;
    }
    case Item::Kind::EXT: {
      assert(!"not implemented");
      return false;
    }
    case Item::Kind::NODE: {
      if (item.off_) {
        for (auto &other : items_) {
          if (other.kind_ != Item::Kind::NODE) {
            continue;
          }
          if (other.nodeVal_ != item.nodeVal_) {
            continue;
          }
          if (!other.off_ || other.off_ == item.off_) {
            return false;
          }
        }
        items_.emplace_back(item.nodeVal_, *item.off_);
      } else {
        for (auto it = items_.begin(); it != items_.end(); ) {
          if (it->kind_ != Item::Kind::NODE) {
            ++it;
            continue;
          }
          if (it->nodeVal_ != item.nodeVal_) {
            ++it;
            continue;
          }
          if (!it->off_) {
            return false;
          }

          *it = *items_.rbegin();
          items_.pop_back();
        }
        items_.emplace_back(item.nodeVal_);
      }
      return true;
    }
  }
}

// -----------------------------------------------------------------------------
GlobalContext::GlobalContext(Prog *prog)
{
  std::vector<std::tuple<Atom *, DataNode *, unsigned>> fixups;

  unsigned offset = 0;
  DataNode *chunk;
  for (auto *data : prog->data()) {
    for (auto &atom : *data) {
      chunk = chunk ? chunk : new DataNode(&atom);
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
      return solver.Ptr(solver.Bag(BuildCamlNode(8)), false);
    }
    if (name == "caml_alloc2") {
      return solver.Ptr(solver.Bag(BuildCamlNode(16)), false);
    }
    if (name == "caml_alloc3") {
      return solver.Ptr(solver.Bag(BuildCamlNode(24)), false);
    }
    if (name == "caml_allocN") {
      return solver.Ptr(solver.Bag(BuildCamlNode(AllocSize())), false);
    }
    if (name == "caml_alloc") {
      return solver.Ptr(solver.Bag(new SetNode()), false);
    }
    if (name == "caml_alloc_small") {
      return solver.Ptr(solver.Bag(new SetNode()), false);
    }
    if (name == "caml_fl_allocate") {
      return solver.Ptr(solver.Bag(new SetNode()), false);
    }
    if (name == "malloc") {
      return solver.Ptr(solver.Bag(new SetNode()), false);
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
          Map(inst, solver.Ptr(solver.Bag(), false));
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

  std::set<std::pair<Constraint *, Constraint *>> subsets;
  for (auto &block : *func) {
    for (auto &phi : block.phis()) {
      std::set<Constraint *> constraints;
      for (unsigned i = 0; i < phi.GetNumIncoming(); ++i) {
        if (auto *c = ValConstraint(phi.GetValue(i))) {
          subsets.emplace(c, Lookup(&phi));
        }
      }
    }
  }

  for (auto &subset : subsets) {
    solver.Subset(subset.first, subset.second);
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
