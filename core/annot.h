// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <llvm/ADT/ilist_node.h>
#include <llvm/ADT/ilist.h>
#include <llvm/ADT/iterator_range.h>


/**
 * Base class for annotations.
 */
class Annot : public llvm::ilist_node<Annot> {
public:
  enum class Kind {
    CAML_FRAME  = 0,
    PROBABILITY = 1,
  };

public:
  /// Creates a new annotation.
  Annot(Kind kind) : kind_(kind) {}

  /// Checks if the annotation is of a given kind.
  bool Is(Kind kind) const { return GetKind() == kind; }
  /// Returns the annotation kind.
  Kind GetKind() const { return kind_; }

  /// Checks if two annotations are equal.
  bool operator==(const Annot &annot) const;

private:
  /// Kind of the annotation.
  Kind kind_;
};


/**
 * Class representing a set of annotations.
 */
class AnnotSet {
private:
  /// Underlying annotation list.
  using AnnotListType = llvm::ilist<Annot>;

public:
  /// Iterator over the annotations.
  using iterator = AnnotListType::iterator;
  using const_iterator = AnnotListType::const_iterator;

public:
  /// Creats a new, empty annotation set.
  AnnotSet();
  /// Moves an annotation set.
  AnnotSet(AnnotSet &&that);
  /// Copies an annotation set.
  AnnotSet(const AnnotSet &that);

  /// Destroys the annotation set.
  ~AnnotSet();

  /**
   * Checks if an annotation is set.
   */
  template<typename T>
  bool Has() const
  {
    for (auto &annot : annots_) {
      if (annot.Is(T::kAnnotKind)) {
        return true;
      }
    }
    return false;
  }

  /**
   * Creates an annotation.
   *
   * @return false if an annotation of the same kind exists.
   */
  template<typename T, typename... Args>
  bool Set(Args&&... args)
  {
    for (auto &annot : annots_) {
      if (annot.Is(T::kAnnotKind)) {
        return false;
      }
    }

    annots_.push_back(new T(std::move(args)...));
    return true;
  }

  /**
   * Clears an annotation.
   */
  template<typename T>
  bool Clear()
  {
    for (auto it = annots_.begin(); it != annots_.end(); ) {
      auto jt = it++;
      if (jt->Is(T::kAnnotKind)) {
        annots_.erase(jt);
        return true;
      }
    }
    return false;
  }

  /**
   * Returns a pointer to an annotation.
   */
  template<typename T>
  const T *Get() const
  {
    for (auto it = annots_.begin(); it != annots_.end(); ++it) {
      if (it->Is(T::kAnnotKind)) {
        return static_cast<const T *>(&*it);
      }
    }
    return nullptr;
  }

  /**
   * Adds an annotation to this set.
   */
  bool Add(const Annot &annot);

  /// Compares two annotations sets for equality.
  bool operator == (const AnnotSet &that) const;
  /// Compares two annotations sets for inequality.
  bool operator != (const AnnotSet &that) const { return !(*this == that); }

  /// Assigns annotation from a different set.
  AnnotSet &operator=(AnnotSet &&that);

  /// Returns the number of set annotations.
  size_t size() const { return annots_.size(); }
  /// Checks if there are any annotations set.
  bool empty() const { return annots_.empty(); }
  /// Iterator to the first annotation.
  iterator begin() { return annots_.begin(); }
  /// Iterator past the last annotation.
  iterator end() { return annots_.end(); }
  /// Constant iterator to the first annotation.
  const_iterator begin() const { return annots_.begin(); }
  /// Constant iterator past the last annotation.
  const_iterator end() const { return annots_.end(); }

private:
  /// Mask indicating which annotations are set.
  AnnotListType annots_;
};

/**
 * OCaml: annotates an instruction which has an entry in the frame table.
 */
class CamlFrame final : public Annot {
public:
  static constexpr Annot::Kind kAnnotKind = Kind::CAML_FRAME;

public:
  /// Debug information.
  struct DebugInfo {
    /// Packed location information.
    int64_t Location;
    /// Name of the originating file.
    std::string File;
    /// Name of the definition.
    std::string Definition;

    /// Compares two debug info objects.
    bool operator==(const DebugInfo &that) const {
      if (Location != that.Location) {
        return false;
      }
      if (File != that.File) {
        return false;
      }
      if (Definition != that.Definition) {
        return false;
      }
      return true;
    }
  };

  /// Debug information bundle.
  using DebugInfos = std::vector<DebugInfo>;

  /// Iterator over allocations.
  using const_alloc_iterator = std::vector<size_t>::const_iterator;
  /// Iterator over debug infos.
  using const_debug_infos_iterator = std::vector<DebugInfos>::const_iterator;

public:
  /// Constructs an annotation without debug info.
  CamlFrame() : Annot(Kind::CAML_FRAME) {}
  /// Constructs an annotation with debug info.
  CamlFrame(
      std::vector<size_t> &&allocs,
      std::vector<DebugInfos> &&debug_infos
  );

  /// Returns the number of allocations.
  size_t alloc_size() const { return allocs_.size(); }
  /// Iterator over allocations.
  llvm::iterator_range<const_alloc_iterator> allocs() const
  {
    return { allocs_.begin(), allocs_.end() };
  }

  /// Returns the number of debug infos.
  size_t debug_info_size() const { return debug_infos_.size(); }
  /// Iterator over debug information bundles.
  llvm::iterator_range<const_debug_infos_iterator> debug_infos() const
  {
    return { debug_infos_.begin(), debug_infos_.end() };
  }

  /// Checks if two annotations are equal.
  bool operator==(const CamlFrame &annot) const;

private:
  /// Sizes of the underlying allocations.
  std::vector<size_t> allocs_;
  /// Debug information objects.
  std::vector<DebugInfos> debug_infos_;
};

/**
 * Annotates a conditional jump with the probability of the taken branch.
 */
class Probability final : public Annot {
public:
  static constexpr Annot::Kind kAnnotKind = Kind::PROBABILITY;

public:
  /// Constructs an annotation carrying a probability.
  Probability(uint32_t n, uint32_t d)
    : Annot(Kind::PROBABILITY), n_(n), d_(d) {}

  /// Returns the numerator.
  float GetNumerator() const { return n_; };
  /// Returns the denominator.
  uint32_t GetDenumerator() const { return d_; }

private:
  /// Numerator.
  uint32_t n_;
  /// Denominator.
  uint32_t d_;
};
