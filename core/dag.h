// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <vector>

#include <llvm/ADT/DenseSet.h>
#include <llvm/Support/raw_ostream.h>

class Block;
class Func;



/**
 * A node in the SCC graph of a function.
 */
struct DAGBlock {
  /// Index of the DAG block (higher is closer to entry).
  const unsigned Index;

  /// Blocks which are part of the collapsed node.
  llvm::DenseSet<Block *> Blocks;
  /// Set of successor nodes.
  llvm::SmallVector<DAGBlock *, 4> Succs;
  /// Set of predecessor nodes.
  llvm::SmallVector<DAGBlock *, 4> Preds;
  /// Length of the longest path to an exit.
  int64_t Length;
  /// Flag indicating whether this is a loop to be over-approximated.
  bool IsLoop;
  /// Flag to indicate whether the node has landing pads.
  bool Lands;
  /// Flag to indicate whether the node is on a path to return.
  bool Returns;
  /// Flag to indicate whether the node is a return.
  bool IsReturn;
  /// Flag to indicate whether the node raises.
  bool Raises;
  /// Flag to indicate whether the node is a raise.
  bool IsRaise;
  /// Node leads to a trap.
  bool Traps;
  /// Node traps.
  bool IsTrap;

  /// Checks whether the node exits.
  bool Exits() const
  {
    return Returns || Raises || Traps;
  }

  /// Checks whether the node is an exit.
  bool IsExit() const
  {
    return IsReturn || IsRaise || IsTrap;
  }

  /// Creates a DAG block.
  DAGBlock(unsigned index) : Index(index) {}
};

/**
 * Print the eval node to a stream.
 */
llvm::raw_ostream &operator<<(llvm::raw_ostream &os, DAGBlock &node);



/**
 * A class which carries information about the SCCs in a function.
 */
class DAGFunc {
public:
  using NodeList = std::vector<std::unique_ptr<DAGBlock>>;

  template <typename T>
  struct iterator : llvm::iterator_adaptor_base
      < iterator<T>
      , T
      , std::random_access_iterator_tag
      , DAGBlock *
      >
  {
    iterator<T>(T it)
      : llvm::iterator_adaptor_base
          < iterator<T>
          , T
          , std::random_access_iterator_tag
          , DAGBlock *
          >(it)
    {
    }

    DAGBlock *operator*() const { return this->I->get(); }
    DAGBlock *operator->() const { return operator*(); }
  };

  using node_iterator = iterator<NodeList::iterator>;
  using reverse_node_iterator = iterator<NodeList::reverse_iterator>;

public:
  DAGFunc(Func &func);

  DAGBlock *operator[] (Block *block) { return blocks_[block]; }
  DAGBlock *operator[] (unsigned idx) { return &*nodes_[idx]; }

  Func &GetFunc() { return func_; }

  // Iterator over nodes.
  size_t size() const { return nodes_.size(); }
  node_iterator begin() { return node_iterator(nodes_.begin()); }
  node_iterator end() { return node_iterator(nodes_.end()); }
  reverse_node_iterator rbegin() { return reverse_node_iterator(nodes_.rbegin()); }
  reverse_node_iterator rend() { return reverse_node_iterator(nodes_.rend()); }

private:
  /// Underlying function.
  Func &func_;
  /// Representation of all strongly-connected components.
  NodeList nodes_;
  /// Mapping from blocks to SCC nodes.
  std::unordered_map<Block *, DAGBlock *> blocks_;
};
