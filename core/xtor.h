// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <memory>

#include <llvm/ADT/ilist_node.h>

#include "core/value.h"

class Global;
class Func;
class Prog;



/// Constructor/Destructor information.
class Xtor : public llvm::ilist_node_with_parent<Xtor, Prog> {
public:
  /// Constructor/destructor kinds.
  enum class Kind {
    CTOR,
    DTOR,
  };

public:
  Xtor(int priority, Global *g, Kind k);

  /// Returns the priority.
  int GetPriority() const { return priority_; }
  /// Return the function.
  Func *GetFunc() const;
  /// Return the kind.
  Kind GetKind() const { return kind_; }

  /// Removes an atom from the data section.
  void removeFromParent();
  /// Removes an parent from the data section, erasing it.
  void eraseFromParent();
  /// Returns a pointer to the parent program.
  Prog *getParent() const { return parent_; }

private:
  /// Priority.
  int priority_;
  /// Reference to the global.
  std::shared_ptr<Use> func_;
  /// Constructor/destructor kind.
  Kind kind_;
  /// Parent program.
  Prog *parent_;
};
