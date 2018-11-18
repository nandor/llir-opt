// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include "insts.h"



/**
 * LoadInst
 */
class LoadInst final : public MemoryInst {
public:
  LoadInst(Block *block, size_t size, Type type, Value *addr);

  /// Returns the number of return values.
  unsigned GetNumRets() const override;
  /// Returns the type of the ith return value.
  Type GetType(unsigned i) const override;
  /// Returns the size of the instruction.
  std::optional<size_t> GetSize() const override;

  /// Returns the type of the load.
  Type GetType() const { return type_; }
  /// Returns the size of the read.
  size_t GetLoadSize() const { return size_; }
  /// Returns the address instruction.
  const Inst *GetAddr() const;

private:
  /// Size of the load.
  size_t size_;
  /// Type of the instruction.
  Type type_;
};

/**
 * StoreInst
 */
class StoreInst final : public MemoryInst {
public:
  StoreInst(Block *block, size_t size, Inst *addr, Inst *val);

  /// Returns the number of return values.
  unsigned GetNumRets() const override;
  /// Returns the type of the ith return value.
  Type GetType(unsigned i) const override;
  /// Returns the size of the instruction.
  std::optional<size_t> GetSize() const override;

  /// Returns the size of the store.
  size_t GetStoreSize() const { return size_; }
  /// Returns the address to store the value at.
  const Inst *GetAddr() const;
  /// Returns the value to store.
  const Inst *GetVal() const;

private:
  /// Size of the store.
  size_t size_;
};

/**
 * PushInst
 */
class PushInst final : public StackInst {
public:
  PushInst(Block *block, Type type, Inst *val);

  /// Returns the number of return values.
  unsigned GetNumRets() const override;
  /// Returns the type of the ith return value.
  Type GetType(unsigned i) const override;
};

/**
 * PopInst
 */
class PopInst final : public StackInst {
public:
  PopInst(Block *block, Type type)
    : StackInst(Kind::POP, block, 0)
    , type_(type)
  {
  }

  /// Returns the number of return values.
  unsigned GetNumRets() const override;
  /// Returns the type of the ith return value.
  Type GetType(unsigned i) const override;

private:
  /// Type of the instruction.
  Type type_;
};

/**
 * ExchangeInst
 */
class ExchangeInst final : public MemoryInst {
public:
  ExchangeInst(
      Block *block,
      Type type,
      Inst *addr,
      Inst *val
  );

  /// Returns the number of return values.
  unsigned GetNumRets() const override;
  /// Returns the type of the ith return value.
  Type GetType(unsigned i) const override;

private:
  /// Type of the instruction.
  Type type_;
};
