// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <llvm/Support/raw_ostream.h>

#include <set>
#include <unordered_set>
#include <unordered_map>
#include <variant>

class Block;
class CallSite;
class Func;
class Global;
class SymbolicPointer;


/**
 * Symbolic address wrapper, used to iterate across pointer contents.
 */
class SymbolicAddress final {
public:
  /// Enumeration of address kinds.
  enum class Kind : uint8_t {
    GLOBAL,
    GLOBAL_RANGE,
    FRAME,
    FRAME_RANGE,
    HEAP,
    HEAP_RANGE,
    FUNC,
    BLOCK,
    STACK,
  };

  /// Exact global address.
  class AddrGlobal {
  public:
    /// Kind of the symbol.
    Kind K;
    /// Global symbol.
    Global *Symbol;
    /// Offset into the symbol.
    int64_t Offset;

  private:
    friend class SymbolicAddress;

    AddrGlobal(Global *symbol, int64_t offset)
        : K(Kind::GLOBAL), Symbol(symbol), Offset(offset)
    {
    }
  };

  /// Range of an entire object.
  class AddrGlobalRange {
  public:
    /// Kind of the symbol.
    Kind K;
    /// Global symbol.
    Global *Symbol;

  private:
    friend class SymbolicAddress;

    AddrGlobalRange(Global *symbol)
        : K(Kind::GLOBAL_RANGE), Symbol(symbol)
    {
    }
  };

  /// Exact frame address.
  class AddrFrame {
  public:
    /// Kind of the symbol.
    Kind K;
    /// ID of the frame.
    unsigned Frame;
    /// ID of the object.
    unsigned Object;
    /// Offset into the symbol.
    int64_t Offset;

  private:
    friend class SymbolicAddress;

    AddrFrame(unsigned frame, unsigned object, int64_t offset)
        : K(Kind::FRAME), Frame(frame), Object(object), Offset(offset)
    {
    }
  };

  /// Range of an entire object.
  class AddrFrameRange {
  public:
    /// Kind of the symbol.
    Kind K;
    /// ID of the frame.
    unsigned Frame;
    /// ID of the object.
    unsigned Object;

  private:
    friend class SymbolicAddress;

    AddrFrameRange(unsigned frame, unsigned object)
        : K(Kind::FRAME_RANGE), Frame(frame), Object(object)
    {
    }
  };

  /// Exact heap address.
  class AddrHeap {
  public:
    /// Kind of the symbol.
    Kind K;
    /// ID of the frame which allocated.
    unsigned Frame;
    /// Allocation site.
    CallSite *Alloc;
    /// Offset into the allocation.
    int64_t Offset;

  private:
    friend class SymbolicAddress;

    AddrHeap(unsigned frame, CallSite *alloc, int64_t offset)
        : K(Kind::HEAP), Frame(frame),Alloc(alloc), Offset(offset)
    {
    }
  };

  /// Exact heap address.
  class AddrHeapRange {
  public:
    /// Kind of the symbol.
    Kind K;
    /// ID of the frame which allocated.
    unsigned Frame;
    /// Allocation site.
    CallSite *Alloc;

  private:
    friend class SymbolicAddress;

    AddrHeapRange(unsigned frame, CallSite *alloc)
        : K(Kind::HEAP_RANGE), Frame(frame), Alloc(alloc)
    {
    }
  };

  /// Pointer to a function.
  class AddrFunc {
  public:
    /// Kind of the symbol.
    Kind K;
    /// Pointer to the function.
    Func *Fn;

  private:
    friend class SymbolicAddress;

    AddrFunc(Func *func) : K(Kind::FUNC), Fn(func) { }
  };

  /// Pointer to a block.
  class AddrBlock {
  public:
    /// Kind of the symbol.
    Kind K;
    /// Pointer to the function.
    Block *B;

  private:
    friend class SymbolicAddress;

    AddrBlock(Block *block) : K(Kind::BLOCK), B(block) { }
  };

  /// Pointer to a stack frame.
  class AddrStack {
  public:
    /// Kind of the symbol.
    Kind K;
    /// Index of the frame.
    unsigned Frame;

  private:
    friend class SymbolicAddress;

    AddrStack(unsigned frame) : K(Kind::STACK), Frame(frame) { }
  };

public:
  /// Construct an address to a specific location.
  SymbolicAddress(std::pair<Global *, int64_t> arg)
    : v_(arg.first, arg.second)
  {
  }
  /// Construct an address to a specific location.
  SymbolicAddress(Global *arg)
    : v_(arg)
  {
  }
  /// Constructs an address inside a frame object.
  SymbolicAddress(std::pair<std::pair<unsigned, unsigned>, int64_t> arg)
    : v_(arg.first.first, arg.first.second, arg.second)
  {
  }
  /// Constructs an address to a frame object.
  SymbolicAddress(std::pair<unsigned, unsigned> arg)
    : v_(arg.first, arg.second)
  {
  }
  /// Constructs an address inside a heap object.
  SymbolicAddress(std::pair<std::pair<unsigned, CallSite *>, int64_t> arg)
    : v_(arg.first.first, arg.first.second, arg.second)
  {
  }
  /// Constructs an address to a frame object.
  SymbolicAddress(std::pair<unsigned, CallSite *> arg)
    : v_(arg.first, arg.second)
  {
  }
  /// Constructs an address to a frame object.
  SymbolicAddress(Func *func) : v_(func) {}
  /// Constructs an address to a block.
  SymbolicAddress(Block *block) : v_(block) {}
  /// Constructs an address to a stack frame.
  SymbolicAddress(unsigned stack) : v_(stack) {}

  /// Returns the address kind.
  Kind GetKind() const { return v_.K; }

  /// Access the actual pointer kind.
  const AddrGlobal &AsGlobal() const { return v_.G; }
  const AddrGlobalRange &AsGlobalRange() const { return v_.GR; }
  const AddrFrame &AsFrame() const { return v_.F; }
  const AddrFrameRange &AsFrameRange() const { return v_.FR; }
  const AddrHeap &AsHeap() const { return v_.H; }
  const AddrHeapRange &AsHeapRange() const { return v_.HR; }
  const AddrFunc &AsFunc() const { return v_.Fn; }
  const AddrBlock &AsBlock() const { return v_.B; }
  const AddrStack &AsStack() const { return v_.Stk; }

  /// Attempt to convert to a global.
  const AddrGlobal *ToGlobal() const
  {
    return v_.K == Kind::GLOBAL ? &v_.G : nullptr;
  }

  /// Attempt to convert to a global.
  const AddrGlobalRange *ToGlobalRange() const
  {
    return v_.K == Kind::GLOBAL_RANGE ? &v_.GR : nullptr;
  }

  /// Attempt to convert to a global.
  const AddrHeap *ToHeap() const
  {
    return v_.K == Kind::HEAP ? &v_.H : nullptr;
  }

  /// Compares two sets of pointers for equality.
  bool operator==(const SymbolicAddress &that) const;

  /// Prints the address.
  void dump(llvm::raw_ostream &os) const;

private:
  /// Base symbol.
  union S {
    Kind K;
    AddrGlobal G;
    AddrGlobalRange GR;
    AddrFrame F;
    AddrFrameRange FR;
    AddrFunc Fn;
    AddrHeap H;
    AddrHeapRange HR;
    AddrBlock B;
    AddrStack Stk;

    S(Global *symbol, int64_t off) : G(symbol, off) { }
    S(Global *symbol) : GR(symbol) { }

    S(unsigned frame, unsigned object, int64_t off) : F(frame, object, off) {}
    S(unsigned frame, unsigned object) : FR(frame, object) { }

    S(unsigned frame, CallSite *site, int64_t off) : H(frame, site, off) { }
    S(unsigned frame, CallSite *site) : HR(frame, site) { }

    S(Func *func) : Fn(func) { }
    S(Block *block) : B(block) { }
    S(unsigned stack) : Stk(stack) { }
  } v_;
};

/// Print the pointer to a stream.
inline llvm::raw_ostream &operator<<(
    llvm::raw_ostream &os,
    const SymbolicAddress &address)
{
  address.dump(os);
  return os;
}


/**
 * An address or a range of addresses.
 */
class SymbolicPointer final {
public:
  using GlobalMap = std::unordered_map<Global *, int64_t>;
  using GlobalRangeMap = std::unordered_set<Global *>;
  using FrameKey = std::pair<unsigned, unsigned>;
  using FrameMap = std::unordered_map<FrameKey, int64_t>;
  using FrameRangeMap = std::unordered_set<FrameKey>;
  using HeapKey = std::pair<unsigned, CallSite *>;
  using HeapMap = std::unordered_map<HeapKey, int64_t>;
  using HeapRangeMap = std::unordered_set<HeapKey>;
  using FuncMap = std::unordered_set<Func *>;
  using BlockMap = std::unordered_set<Block *>;
  using StackMap = std::unordered_set<unsigned>;

  using func_iterator = FuncMap::const_iterator;

  class address_iterator : public std::iterator<std::forward_iterator_tag, SymbolicAddress> {
  public:
    address_iterator()
      : pointer_(nullptr)
      , it_(pointer_->frameRanges_.end())
    {
    }

    template <typename It>
    address_iterator(It it, const SymbolicPointer *pointer)
      : pointer_(pointer)
      , it_(it)
      , current_(SymbolicAddress(*it))
    {
    }

    bool operator==(const address_iterator &that) const
    {
      return current_ == that.current_;
    }
    bool operator!=(const address_iterator &that) const
    {
      return !(*this == that);
    }

    address_iterator &operator++();
    address_iterator operator++(int)
    {
      auto tmp = *this;
      ++*this;
      return tmp;
    }

    const SymbolicAddress &operator*() const { return *current_; }
    const SymbolicAddress *operator->() const { return &operator*(); }

  private:
    /// Reference to the parent pointer.
    const SymbolicPointer *pointer_;
    /// On-the-fly address.
    std::optional<SymbolicAddress> current_;
    /// Current iterator.
    std::variant<
        GlobalMap::const_iterator,
        GlobalRangeMap::const_iterator,
        FrameMap::const_iterator,
        FrameRangeMap::const_iterator,
        HeapMap::const_iterator,
        HeapRangeMap::const_iterator,
        FuncMap::const_iterator,
        BlockMap::const_iterator,
        StackMap::const_iterator
    > it_;
  };

public:
  SymbolicPointer();
  SymbolicPointer(Func *func);
  SymbolicPointer(Block *block);
  SymbolicPointer(unsigned frame);
  SymbolicPointer(Global *symbol, int64_t offset);
  SymbolicPointer(unsigned frame, unsigned object, int64_t offset);
  SymbolicPointer(unsigned frame, CallSite *alloc, int64_t offset);
  SymbolicPointer(const SymbolicPointer &that);
  SymbolicPointer(SymbolicPointer &&that);
  ~SymbolicPointer();

  /// Compares two sets of pointers for equality.
  bool operator==(const SymbolicPointer &that) const;

  /// Add a global to the pointer.
  void Add(Global *g) { globalRanges_.insert(g); }
  /// Add a function to the pointer.
  void Add(Func *f) { funcPointers_.insert(f); }
  /// Adds a block to the pointer.
  void Add(Block *b) { blockPointers_.insert(b); }
  /// Add a heap object to the pointer.
  void Add(unsigned frame, CallSite *a);
  /// Adds a frame object to the pointer.
  void Add(unsigned frame, unsigned object);
  /// Adds a stack frame to the pointer.
  void Add(unsigned frame) { stackPointers_.insert(frame); }

  /// Offset the pointer.
  SymbolicPointer Offset(int64_t offset) const;
  /// Decays the pointer to ranges.
  SymbolicPointer Decay() const;

  /// Computes the least-upper-bound.
  void LUB(const SymbolicPointer &that);

  /// Dump the textual representation to a stream.
  void dump(llvm::raw_ostream &os) const;

  /// Checks whether the pointer points to anything.
  bool empty() const { return begin() == end(); }
  /// Iterator over pointers contained in the set.
  address_iterator begin() const;
  address_iterator end() const { return address_iterator(); }

  /// Iterator over functions.
  size_t func_size() const { return std::distance(func_begin(), func_end()); }
  func_iterator func_begin() const { return funcPointers_.begin(); }
  func_iterator func_end() const { return funcPointers_.end(); }
  llvm::iterator_range<func_iterator> funcs() const
  {
    return llvm::make_range(func_begin(), func_end());
  }

private:
  friend class address_iterator;
  /// Set of direct global pointers.
  GlobalMap globalPointers_;
  /// Set of imprecise global ranges.
  GlobalRangeMap globalRanges_;
  /// Set of direct frame pointers.
  FrameMap framePointers_;
  /// Set of imprecise frame pointers.
  FrameRangeMap frameRanges_;
  /// Set of precise heap pointer.
  HeapMap heapPointers_;
  /// Set of heap pointer ranges.
  HeapRangeMap heapRanges_;
  /// Set of functions.
  FuncMap funcPointers_;
  /// Set of blocks.
  BlockMap blockPointers_;
  /// Set of stack frames.
  StackMap stackPointers_;
};

/// Print the pointer to a stream.
inline llvm::raw_ostream &operator<<(
    llvm::raw_ostream &os,
    const SymbolicPointer &sym)
{
  sym.dump(os);
  return os;
}
