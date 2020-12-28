// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <llvm/ADT/GraphTraits.h>
#include <llvm/Support/DOTGraphTraits.h>

#include "core/block.h"
#include "core/func.h"



/// Traits for blocks.
template <>
struct llvm::GraphTraits<Block *> {
  using NodeRef = Block *;
  using ChildIteratorType = Block::succ_iterator;

  static NodeRef getEntryNode(Block *BB) { return BB; }
  static ChildIteratorType child_begin(NodeRef N) { return N->succ_begin(); }
  static ChildIteratorType child_end(NodeRef N) { return N->succ_end(); }
};

template <>
struct llvm::GraphTraits<const Block *> {
  using NodeRef = const Block *;
  using ChildIteratorType = Block::const_succ_iterator;

  static NodeRef getEntryNode(const Block *BB) { return BB; }
  static ChildIteratorType child_begin(NodeRef N) { return N->succ_begin(); }
  static ChildIteratorType child_end(NodeRef N) { return N->succ_end(); }
};


/// Reverse traits for blocks.
template <>
struct llvm::GraphTraits<llvm::Inverse<Block*>> {
  using NodeRef = Block *;
  using ChildIteratorType = Block::pred_iterator;

  static NodeRef getEntryNode(llvm::Inverse<Block *> G) { return G.Graph; }
  static ChildIteratorType child_begin(NodeRef N) { return N->pred_begin(); }
  static ChildIteratorType child_end(NodeRef N) { return N->pred_end(); }
};

template <>
struct llvm::GraphTraits<llvm::Inverse<const Block*>> {
  using NodeRef = const Block *;
  using ChildIteratorType = const Block::const_pred_iterator;

  static NodeRef getEntryNode(llvm::Inverse<const Block *> G) { return G.Graph; }
  static ChildIteratorType child_begin(NodeRef N) { return N->pred_begin(); }
  static ChildIteratorType child_end(NodeRef N) { return N->pred_end(); }
};

/// Traits for functions.
template <>
struct llvm::GraphTraits<Func *> : public llvm::GraphTraits<Block *> {
  using nodes_iterator = pointer_iterator<Func::iterator>;

  static NodeRef getEntryNode(Func *F) { return &F->getEntryBlock(); }
  static nodes_iterator nodes_begin(Func *F) {
    return nodes_iterator(F->begin());
  }

  static nodes_iterator nodes_end(Func *F) {
    return nodes_iterator(F->end());
  }

  static size_t size(Func *F) { return F->size(); }
};

template<>
struct llvm::DOTGraphTraits<Func*> : public llvm::DefaultDOTGraphTraits {
  DOTGraphTraits(bool isSimple = false) : DefaultDOTGraphTraits(isSimple) {}

  static std::string getNodeLabel(const Block *block, const Func *f)
  {
    return std::string(block->getName());
  }

  static std::string getNodeAttributes(const Block *block, const Func *f)
  {
    std::string attrsStr;
    llvm::raw_string_ostream os(attrsStr);
    switch (block->GetTerminator()->GetKind()) {
      default: break;
      case Inst::Kind::TRAP: {
        os << "color=red";
        break;
      }
    }
    return attrsStr;
  }
};

template <>
struct llvm::GraphTraits<const Func *> : public llvm::GraphTraits<const Block *> {
  using nodes_iterator = pointer_iterator<Func::const_iterator>;

  static NodeRef getEntryNode(const Func *F) { return &F->getEntryBlock(); }
  static nodes_iterator nodes_begin(const Func *F) {
    return nodes_iterator(F->begin());
  }

  static nodes_iterator nodes_end(const Func *F) {
    return nodes_iterator(F->end());
  }

  static size_t size(const Func *F) { return F->size(); }
};

/// Reverse traits for functions.
template <>
struct llvm::GraphTraits<llvm::Inverse<Func*>> : public llvm::GraphTraits<llvm::Inverse<Block*>> {
  static NodeRef getEntryNode(llvm::Inverse<Func *> G) {
    return &G.Graph->getEntryBlock();
  }
};

template <>
struct llvm::GraphTraits<llvm::Inverse<const Func*>> : public llvm::GraphTraits<llvm::Inverse<const Block*>> {
  static NodeRef getEntryNode(llvm::Inverse<const Func *> G) {
    return &G.Graph->getEntryBlock();
  }
};
