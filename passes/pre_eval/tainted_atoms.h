// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <set>
#include <unordered_map>

class Func;
class Block;



/**
 * Block-level tainted atom analysis.
 */
class TaintedAtoms final {
public:
  struct Tainted {
  public:
    struct All {};

    /// Constructs an empty set.
    Tainted() : all_(false) {}
    /// Constructs a full set.
    Tainted(All) : all_(true) {}

    /// Merges all elements from the other set into this.
    bool Union(const Tainted &that);

    /// Adds an element to the set.
    bool Add(Atom *atom);

    /// Checks if the set taints everything.
    bool Full() const { return all_; }

  private:
    /// Set of individual tainted atoms.
    std::set<Atom *> atoms_;
    /// Symbolic representation for a set tainting all.
    bool all_;
  };

public:
  /**
   * Runs the analysis, using func as the entry.
   */
  TaintedAtoms(const Func &func);

  /**
   * Cleanup.
   */
  ~TaintedAtoms();

  /**
   * Returns the set of tainted atoms reaching a block.
   */
  const Tainted *operator[](const Block &block) const;

private:
  /// Visits a node, returning the block at the end of it.
  void Visit(const Block &block, const Tainted &vals);
  /// Returns the outgoing information for a function.
  Tainted &Exit(const Func &block);

private:
  struct BlockInfo {
    /// Information into the block.
    Tainted Entry;
    /// Information out of the block.
    Tainted Exit;

    /// Empty set constructor.
    BlockInfo() {}

    /// Construct from specific sets.
    BlockInfo(const Tainted &entry, const Tainted &exit)
      : Entry(entry), Exit(exit)
    {}
  };

  /// Information at function exit points.
  std::unordered_map<const Func *, std::unique_ptr<Tainted>> exits_;
  /// Information for individual blocks.
  std::unordered_map<const Block *, std::unique_ptr<BlockInfo>> blocks_;
};
