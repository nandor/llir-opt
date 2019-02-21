// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <map>
#include <unordered_map>

class Atom;



/**
 * Base class of nodes modelling the heap.
 */
class Node {
public:
  Node() { }

  virtual void Load(std::function<void(const Bag::Item&)> &&f) = 0;
  virtual void Load(unsigned off, std::function<void(const Bag::Item&)> &&f) = 0;

  virtual std::optional<unsigned> GetSize() const = 0;

  virtual bool Store(const Bag::Item &item) = 0;
  virtual bool Store(unsigned off, const Bag::Item &item) = 0;
};

/**
 * Simple node, used to represent C allocation points.
 */
class SetNode final : public Node {
public:
  void Load(std::function<void(const Bag::Item&)> &&f) override
  {
    bag_.ForEach(std::forward<decltype(f)>(f));
  }

  void Load(unsigned off, std::function<void(const Bag::Item&)> &&f) override
  {
    return Load(std::forward<std::function<void(const Bag::Item&)>>(f));
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

  void Load(std::function<void(const Bag::Item&)> &&f) override
  {
    for (auto &field : fields_) {
      field.second->ForEach(f);
    }
    if (common_) {
      common_->ForEach(std::forward<decltype(f)>(f));
    }
  }

  void Load(unsigned off, std::function<void(const Bag::Item&)> &&f) override
  {
    unsigned l = (off + 0) & ~7;
    unsigned h = (off + 7) & ~7;
    LoadSlot(l, std::forward<decltype(f)>(f));
    if (h != l) {
      LoadSlot(h, std::forward<decltype(f)>(f));
    }

    if (common_) {
      common_->ForEach(std::forward<decltype(f)>(f));
    }
  }

  std::optional<unsigned> GetSize() const override
  {
    return std::nullopt;
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
    unsigned l = (off + 0) & ~7;
    unsigned h = (off + 7) & ~7;
    bool changed = StoreSlot(l, item);
    if (h != l) {
      changed |= StoreSlot(h, item);
    }
    return changed;
  }

private:
  void LoadSlot(unsigned off, std::function<void(const Bag::Item&)> &&f)
  {
    auto it = fields_.find(off);
    if (it == fields_.end()) {
      return;
    }

    it->second->ForEach(std::forward<decltype(f)>(f));
  }

  bool StoreSlot(unsigned off, const Bag::Item &item)
  {
    auto it = fields_.emplace(off, nullptr);
    if (it.second) {
      it.first->second = std::make_unique<Bag>();
    }
    return it.first->second->Store(item);
  }

private:
  /// Source atom.
  Atom *atom_;
  /// Each field of the global chunk is modelled independently.
  std::map<unsigned, std::unique_ptr<Bag>> fields_;
  /// Bag of items common to all fields.
  std::unique_ptr<Bag> common_;
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

  void Load(std::function<void(const Bag::Item&)> &&f) override
  {
    if (common_) {
      common_->ForEach(f);
    }
    for (auto &field : fields_) {
      field.second->ForEach(f);
    }
  }

  void Load(unsigned off, std::function<void(const Bag::Item&)> &&f) override
  {
    if (off < 8 || off >= size_ * 8) {
      return;
    } else if (off % 8 != 0) {
      return;
    } else {
      auto it = fields_.find(off);
      if (it != fields_.end()) {
        it->second->ForEach(f);
      }
      if (common_) {
        common_->ForEach(std::forward<decltype(f)>(f));
      }
    }
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
    if (off < 8 || size_ * 8 <= off) {
      return false;
    } else if (off % 8 != 0) {
      return false;
    } else {
      auto it = fields_.emplace(off, nullptr);
      if (it.second) {
        it.first->second = std::make_unique<Bag>();
      }
      return it.first->second->Store(item);
    }
  }


private:
  /// Size of the OCaml chunk.
  unsigned size_;
  /// Values everywhere.
  std::unique_ptr<Bag> common_;
  /// Values stored in fields.
  std::unordered_map<unsigned, std::unique_ptr<Bag>> fields_;
};
