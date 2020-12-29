// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <vector>
#include <optional>

#include <llvm/ADT/ilist_node.h>
#include <llvm/ADT/ilist.h>

#include "core/annot.h"
#include "core/constant.h"
#include "core/cond.h"
#include "core/expr.h"
#include "core/type.h"
#include "core/register.h"
#include "core/value.h"

class Block;
class Inst;
class Context;
class Symbol;



/**
 * Traits to handle parent links from instructions.
 */
template <> struct llvm::ilist_traits<Inst> {
private:
  using instr_iterator = simple_ilist<Inst>::iterator;

public:
  void deleteNode(Inst *inst);
  void addNodeToList(Inst *inst);
  void removeNodeFromList(Inst *inst);
  void transferNodesFromList(
      ilist_traits &from,
      instr_iterator first,
      instr_iterator last
  );

  Block *getParent();
};

/**
 * Basic instruction.
 */
class Inst
  : public llvm::ilist_node_with_parent<Inst, Block>
  , public User
{
public:
  /// Kind of the instruction.
  static constexpr Value::Kind kValueKind = Value::Kind::INST;

public:
  /**
   * Enumeration of instruction types.
   */
  enum class Kind : uint8_t {
    #define GET_INST(kind, type, name, sort) kind,
    #include "instructions.def"
  };

  /// Destroys an instruction.
  virtual ~Inst();

  /// Returns a unique, stable identifier for the instruction.
  unsigned GetOrder() const { return order_; }

  /// Removes an instruction from the parent.
  void removeFromParent();
  /// Removes an instruction from the parent and deletes it.
  void eraseFromParent();

  /// Returns the instruction kind.
  Kind GetKind() const { return kind_; }
  /// Checks if the instruction is of a specific kind.
  bool Is(Kind kind) const { return GetKind() == kind; }
  /// Returns the parent node.
  Block *getParent() const { return parent_; }
  /// Returns the number of returned values.
  virtual unsigned GetNumRets() const { return 0; }
  /// Returns the type of the ith return value.
  virtual Type GetType(unsigned i) const { llvm_unreachable("missing type"); }

  /// Checks if the instruction is void.
  bool IsVoid() const { return GetNumRets() == 0; }

  /// Checks if the instruction returns from the function.
  virtual bool IsReturn() const { return false; }
  /// Checks if the instruction is constant.
  virtual bool IsConstant() const { return false; }
  /// Checks if the instruction is a terminator.
  virtual bool IsTerminator() const { return false; }
  /// Checks if the instruction has side effects.
  virtual bool HasSideEffects() const { return false; }

  /// Checks if a flag is set.
  template<typename T>
  bool HasAnnot() const { return annot_.Has<T>(); }

  /// Removes an annotation.
  template<typename T>
  bool ClearAnnot() { return annot_.Clear<T>(); }

  /// Returns an annotation.
  template<typename T>
  const T *GetAnnot() const { return annot_.Get<T>(); }

  /// Sets an annotation.
  template<typename T, typename... Args>
  bool SetAnnot(Args&&... args)
  {
    return annot_.Set<T, Args...>(std::forward<Args>(args)...);
  }

  /// Adds an annotation.
  bool AddAnnot(const Annot &annot) { return annot_.Add(annot); }
  /// Returns the instruction's annotation.
  const AnnotSet &GetAnnots() const { return annot_; }
  /// Returns the number of annotations.
  size_t annot_size() const { return annot_.size(); }
  /// Checks if any flags are set.
  bool annot_empty() const { return annot_.empty(); }
  /// Iterator over annotations.
  llvm::iterator_range<AnnotSet::const_iterator> annots() const
  {
    return llvm::make_range(annot_.begin(), annot_.end());
  }

  /// Returns the ith sub-value.
  Ref<Inst> GetSubValue(unsigned i) { return Ref(this, i); }
  /// Returns the ith sub-value.
  ConstRef<Inst> GetSubValue(unsigned i) const { return ConstRef(this, i); }

  /// Replaces all uses of this value.
  void replaceAllUsesWith(Value *v) override;
  /// Replaces all uses of a multi-type value.
  void replaceAllUsesWith(llvm::ArrayRef<Ref<Inst>> v);

  /// Replaces all uses of a multi-type value.
  template <typename T>
  typename std::enable_if<std::is_base_of<Inst, T>::value>::type
  replaceAllUsesWith(llvm::ArrayRef<Ref<T>> insts)
  {
    std::vector<Ref<Inst>> values;
    for (Ref<T> inst : insts) {
      values.push_back(inst);
    }
    return replaceAllUsesWith(values);
  }

  /// Dumps the textual representation of the instruction.
  void dump(llvm::raw_ostream &os = llvm::errs()) const;

protected:
  /// Constructs an instruction of a given type.
  Inst(Kind kind, unsigned numOps, AnnotSet &&annot);
  /// Constructs an instruction of a given type.
  Inst(Kind kind, unsigned numOps, const AnnotSet &annot);

private:
  friend struct llvm::ilist_traits<Inst>;
  /// Updates the parent node.
  void setParent(Block *parent) { parent_ = parent; }

private:
  /// Instruction kind.
  const Kind kind_;
  /// Instruction annotation.
  AnnotSet annot_;

protected:
  /// Parent node.
  Block *parent_;
  /// Unique number for stable ordering.
  unsigned order_;
};

/// Print the value to a stream.
inline llvm::raw_ostream &operator<<(llvm::raw_ostream &os, const Inst &inst)
{
  inst.dump(os);
  return os;
}

#define GET_BASE_INTF
#include "instructions.def"
