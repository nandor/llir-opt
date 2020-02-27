// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "passes/local_const/graph.h"
#include "passes/local_const/scc.h"



// -----------------------------------------------------------------------------
LCSCC::LCSCC(LCGraph &graph)
  : graph_(graph)
  , epoch_(1ul)
{
}

// -----------------------------------------------------------------------------
LCSCC &LCSCC::Full()
{
  // Reset the traversal.
  epoch_ += 1;
  index_ = 1;

  // Find SCCs rooted at nodes which were not visited.
  for (auto *set : graph_) {
    VisitFull(set);
    if (auto *deref = set->GetDeref()) {
      VisitFull(deref);
    }
  }

  // Stack must be empty by this point.
  assert(stack_.size() == 0);
  return *this;
}

// -----------------------------------------------------------------------------
LCSCC &LCSCC::Single(LCSet *node)
{
  // Reset the traversal.
  epoch_ += 1;
  index_ = 1;

  // Find SCCs starting at this node.
  VisitSingle(node);

  // Stack must be empty after traversal.
  assert(stack_.size() == 0);
  return *this;
}

// -----------------------------------------------------------------------------
void LCSCC::Solve(std::function<void(const SetGroup &, const DerefGroup &)> &&f)
{
  /// Traverse the computed SCCs.
  for (const auto &scc : sccs_) {
    f(scc.first, scc.second);
  }
  sccs_.clear();
}

// -----------------------------------------------------------------------------
void LCSCC::VisitFull(LCSet *node)
{
  auto Visitor = [this, node](auto *next) {
    if (next->Epoch != epoch_) {
      VisitFull(next);
      node->Link = std::min(node->Link, next->Link);
    } else if (!next->InComponent) {
      node->Link = std::min(node->Link, next->Link);
    }
  };

  Pre(node);
  node->sets(Visitor);
  node->ranges(Visitor);
  node->offsets([this, &Visitor](LCSet *s, int64_t) { Visitor(s); });
  node->deref_outs(Visitor);
  Post(node, node->GetID());
}

// -----------------------------------------------------------------------------
void LCSCC::VisitFull(LCDeref *node)
{
  auto Visitor = [this, node](LCSet *next) {
    if (next->Epoch != epoch_) {
      VisitFull(next);
      node->Link = std::min(node->Link, next->Link);
    } else if (!next->InComponent) {
      node->Link = std::min(node->Link, next->Link);
    }
  };

  Pre(node);
  node->set_outs(Visitor);
  Post(node, node->GetID());
}

// -----------------------------------------------------------------------------
void LCSCC::VisitSingle(LCSet *node)
{
  auto Visitor = [this, node](LCSet *next) {
    if (next->Epoch != epoch_) {
      VisitSingle(next);
      node->Link = std::min(node->Link, next->Link);
    } else if (!next->InComponent) {
      node->Link = std::min(node->Link, next->Link);
    }
  };

  Pre(node);
  node->sets(Visitor);
  node->ranges(Visitor);
  node->offsets([this, &Visitor](LCSet *s, int64_t) { Visitor(s); });
  Post(node, node->GetID());
}

// -----------------------------------------------------------------------------
template <typename T>
void LCSCC::Pre(T *node)
{
  node->Epoch = epoch_;
  node->Index = index_;
  node->Link = index_;
  index_ += 1;
  node->InComponent = false;
}

// -----------------------------------------------------------------------------
namespace {

template <typename T, typename U, typename V>
struct Selector {
};

template <typename U, typename V>
struct Selector<ID<LCSet>, U, V> {
  static U &get(std::pair<U, V> &p) { return p.first; }
};

template <typename U, typename V>
struct Selector<ID<LCDeref>, U, V> {
  static V &get(std::pair<U, V> &p) { return p.second; }
};

}

// -----------------------------------------------------------------------------
template <typename T>
void LCSCC::Post(T *node, ID<T> id)
{
  if (node->Link == node->Index) {
    node->InComponent = true;

    std::pair<SetGroup, DerefGroup> *scc = nullptr;

    while (!stack_.empty()) {
      auto id = stack_.top();
      auto *v = std::visit([this](auto &&id) -> LCNode * {
        return graph_.Find(id);
      }, id);
      if (v->Index <= node->Link) {
        break;
      }
      stack_.pop();
      v->InComponent = true;
      if (!scc) {
        scc = &sccs_.emplace_back();
      }
      std::visit([this, scc](auto &&id) {
        using S = std::decay_t<decltype(id)>;
        Selector<S, SetGroup, DerefGroup>::get(*scc).push_back(id);
      }, id);
    }
    if (scc) {
      Selector<ID<T>, SetGroup, DerefGroup>::get(*scc).push_back(id);
    }
  } else {
    stack_.push(node->GetID());
  }
}
