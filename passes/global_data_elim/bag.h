// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <functional>
#include <optional>
#include <set>
#include <utility>

class Node;
class Func;
class Extern;



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
    void Load(std::function<void(const Item&)> &&f) const;

    /// Offsets an item.
    std::optional<Item> Offset(const std::optional<int64_t> &off) const;

    /// Updates the item.
    bool Store(const Item &item) const;

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
    nodes_.emplace(node);
  }

  /// Singleton specific offset.
  Bag(Node *node, unsigned off)
  {
    offs_.emplace(node, off);
  }

  /// Singleton external pointer.
  Bag(Extern *ext)
  {
    exts_.emplace(ext);
  }

  /// Singleton function pointer.
  Bag(Func *func)
  {
    funcs_.emplace(func);
  }

  /// Stores an item into the bag.
  bool Store(const Item &item);

  /// Checks if the bag is empty.
  bool IsEmpty() const
  {
    return funcs_.empty() && exts_.empty() && nodes_.empty() && offs_.empty();
  }

  /// Iterates over nodes.
  void ForEach(const std::function<void(const Item &)> &&f)
  {
    for (auto *func : funcs_) {
      f(Item(func));
    }
    for (auto *ext : exts_) {
      f(Item(ext));
    }
    for (auto *node : nodes_) {
      f(Item(node));
    }
    for (auto &off : offs_) {
      if (nodes_.find(off.first) == nodes_.end()) {
        f(Item(off.first, off.second));
      }
    }
  }

  void ForEach(const std::function<void(const Item &)> &f)
  {
    for (auto *func : funcs_) {
      f(Item(func));
    }
    for (auto *ext : exts_) {
      f(Item(ext));
    }
    for (auto *node : nodes_) {
      f(Item(node));
    }
    for (auto &off : offs_) {
      if (nodes_.find(off.first) == nodes_.end()) {
        f(Item(off.first, off.second));
      }
    }
  }

private:
  /// Stored items.
  std::set<Func *> funcs_;
  /// Stored exts.
  std::set<Extern *> exts_;
  /// Stored nodes.
  std::set<Node *> nodes_;
  /// Nodes with offsets.
  std::set<std::pair<Node *, unsigned>> offs_;
};
