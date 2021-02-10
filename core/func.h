// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <string_view>

#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/ilist_node.h>
#include <llvm/ADT/ilist.h>
#include <llvm/Support/raw_ostream.h>

#include "core/calling_conv.h"
#include "core/global.h"
#include "core/symbol_table.h"
#include "core/type.h"
#include "core/value.h"

class Block;
class Func;
class Prog;



/**
 * GenericMachine function.
 */
class Func final : public llvm::ilist_node_with_parent<Func, Prog>, public Global {
public:
  /// Kind of the global.
  static constexpr Global::Kind kGlobalKind = Global::Kind::FUNC;

public:
  /// Type of the block list.
  using BlockListType = SymbolTableList<Block>;

  /// Iterator over the blocks.
  using iterator = BlockListType::iterator;
  using reverse_iterator = BlockListType::reverse_iterator;
  using const_iterator = BlockListType::const_iterator;

public:
  /// Type of stack objects.
  struct StackObject {
    /// Index of the stack object.
    unsigned Index;
    /// Size of the object on the stack.
    unsigned Size;
    /// Alignment of the object, in bytes.
    llvm::Align Alignment;

    StackObject(unsigned index, unsigned size, llvm::Align alignment)
      : Index(index)
      , Size(size)
      , Alignment(alignment)
    {
    }
  };

  using stack_iterator = std::vector<StackObject>::iterator;

public:
  /**
   * Creates a new function.
   */
  Func(
      const std::string_view name,
      Visibility visibility = Visibility::LOCAL
  );

  /**
   * Destroys the function.
   */
  ~Func() override;

  /// Returns the unique ID.
  unsigned GetID() { return id_; }

  /// Removes an instruction from the parent.
  void removeFromParent() override;
  /// Removes a function from the program.
  void eraseFromParent() override;

  /// Adds a new basic block.
  void AddBlock(Block *block, Block *before = nullptr);

  /// Returns the parent block.
  Prog *getParent() const { return parent_; }

  /// Adds a stack object.
  unsigned AddStackObject(unsigned index, unsigned size, llvm::Align align);
  /// Removes a stack object.
  void RemoveStackObject(unsigned index);

  /// Sets the calling convention.
  void SetCallingConv(CallingConv conv) { callConv_ = conv; }
  /// Returns the calling convention.
  CallingConv GetCallingConv() const { return callConv_; }

  /// Sets the vararg flag.
  void SetVarArg(bool varArg = true) { varArg_ = varArg; }
  /// Returns the vararg flags.
  bool IsVarArg() const { return varArg_; }

  /// Sets the alignment of the function.
  void SetAlignment(llvm::Align align) { align_ = align; }
  /// Returns the alignment of a function.
  std::optional<llvm::Align> GetAlignment() const override { return align_; }

  /// Checks if the function can be inlined.
  bool IsNoInline() const { return noinline_; }
  /// Prevents the function from being inlined.
  void SetNoInline(bool noinline = true) { noinline_ = noinline; }

  /// Returns the function-specific target features.
  std::string_view GetFeatures() const { return features_; }
  llvm::StringRef getFeatures() const { return features_; }
  void SetFeatures(const std::string_view features) { features_ = features; }

  /// Returns the CPU to compile for.
  std::string_view GetCPU() const { return cpu_; }
  llvm::StringRef getCPU() const { return cpu_; }
  void SetCPU(const std::string_view cpu) { cpu_ = cpu; }

  /// Returns the CPU to tune for.
  std::string_view GetTuneCPU() const { return tuneCPU_; }
  llvm::StringRef getTuneCPU() const { return tuneCPU_; }
  void SetTuneCPU(const std::string_view tuneCPU) { tuneCPU_ = tuneCPU; }

  /// Sets the number of fixed parameters.
  void SetParameters(const std::vector<FlaggedType> &params) { params_ = params; }
  /// Returns the list of arguments.
  llvm::ArrayRef<FlaggedType> params() const { return params_; }
  /// Returns the number of parameters.
  unsigned GetNumParams() const { return params_.size(); }

  /// Iterator over stack objects.
  llvm::ArrayRef<StackObject> objects() const { return objects_; }

  /// Finds a stack object by index.
  StackObject &object(unsigned I)
  {
    auto it = objectIndices_.find(I);
    assert(it != objectIndices_.end() && "missing stack object");
    return objects_[it->second];
  }
  /// Finds a stack object by index.
  const StackObject &object(unsigned I) const
  {
    return const_cast<Func *>(this)->object(I);
  }

  /// Removes a block
  void remove(iterator it);
  /// Erases a block.
  void erase(iterator it);
  /// Adds a block.
  void insertAfter(iterator it, Block *block);
  /// Adds a block before another.
  void insert(iterator it, Block *block);
  /// Clears all blocks.
  void clear();

  /// Checks if the function has any blocks.
  bool empty() const { return blocks_.empty(); }

  /// Returns the size of the function.
  size_t size() const { return blocks_.size(); }

  /// Checks if the function can be used indirectly.
  bool HasAddressTaken() const;
  /// Checks if the function must be present even without uses.
  bool IsEntry() const { return IsRoot() || HasAddressTaken(); }
  /// Checks if the function never returns.
  bool DoesNotReturn() const;
  /// Checks if the function has a raise instruction.
  bool HasRaise() const;
  /// Checks if the function has a va_start instruction.
  bool HasVAStart() const;
  /// Checks if the function has indirect calls.
  bool HasIndirectCalls() const;

  /// Returns the entry block.
  Block &getEntryBlock();
  const Block &getEntryBlock() const {
    return const_cast<Func *>(this)->getEntryBlock();
  }

  // Iterator over the blocks.
  iterator begin() { return blocks_.begin(); }
  iterator end() { return blocks_.end(); }
  const_iterator begin() const { return blocks_.begin(); }
  const_iterator end() const { return blocks_.end(); }

  // Reverse iterator over the blocks.
  reverse_iterator rbegin() { return blocks_.rbegin(); }
  reverse_iterator rend() { return blocks_.rend(); }

  // Counts the number of instructions.
  size_t inst_size() const;

  /// Removes unreachable blocks.
  void RemoveUnreachable();

  /// Returns the program to which the function belongs.
  Prog *getProg() override { return parent_; }

  /// Dumps the representation of the function.
  void dump(llvm::raw_ostream &os = llvm::errs()) const;

private:
  friend struct SymbolTableListTraits<Func>;
  friend struct SymbolTableListTraits<Block>;
  /// Updates the parent node.
  void setParent(Prog *parent) { parent_ = parent; }

  static BlockListType Func::*getSublistAccess(Block *) { return &Func::blocks_; }

private:
  /// Unique ID for each function.
  unsigned id_;
  /// Name of the underlying program.
  Prog *parent_;
  /// Chain of basic blocks.
  BlockListType blocks_;
  /// Calling convention used by the function.
  CallingConv callConv_;
  /// Types of parameters.
  std::vector<FlaggedType> params_;
  /// Stack objects.
  std::vector<StackObject> objects_;
  /// Stack objects indices to objects.
  llvm::DenseMap<unsigned, unsigned> objectIndices_;
  /// Vararg flag.
  bool varArg_;
  /// Function alignment.
  std::optional<llvm::Align> align_;
  /// Inline flag.
  bool noinline_;
  /// Target features.
  std::string features_;
  /// Target CPU.
  std::string cpu_;
  /// Target CPU to tune for.
  std::string tuneCPU_;
};
