// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <queue>

#include <llvm/Support/Debug.h>

#include "core/atom.h"
#include "core/object.h"
#include "core/global.h"
#include "core/extern.h"
#include "core/func.h"
#include "core/inst.h"
#include "core/insts.h"
#include "core/cast.h"
#include "passes/pre_eval/symbolic_value.h"
#include "passes/pre_eval/symbolic_context.h"

#define DEBUG_TYPE "pre-eval"



// -----------------------------------------------------------------------------
SymbolicContext::SymbolicContext(Prog &prog)
{
}

// -----------------------------------------------------------------------------
SymbolicContext::SymbolicContext(const SymbolicContext &that)
  : funcs_(that.funcs_)
  , activeFrames_(that.activeFrames_)
  , extern_(that.extern_)
  , executedFunctions_(that.executedFunctions_)
{
  for (auto &[g, object] : that.objects_) {
    objects_.emplace(g, std::make_unique<SymbolicDataObject>(*object));
  }
  for (auto &frame : that.frames_) {
    frames_.emplace_back(frame);
  }
  for (auto &[key, object] : that.allocs_) {
    allocs_.emplace(key, std::make_unique<SymbolicHeapObject>(*object));
  }
}

// -----------------------------------------------------------------------------
SymbolicContext::~SymbolicContext()
{
}

// -----------------------------------------------------------------------------
SymbolicFrame *SymbolicContext::GetActiveFrame()
{
  if (activeFrames_.empty()) {
    return nullptr;
  } else {
    return &frames_[*activeFrames_.rbegin()];
  }
}

// -----------------------------------------------------------------------------
unsigned SymbolicContext::EnterFrame(
    Func &func,
    llvm::ArrayRef<SymbolicValue> args)
{
  unsigned frame = frames_.size();

  #ifndef NDEBUG
  LLVM_DEBUG(llvm::dbgs() << "=======================================\n");
  LLVM_DEBUG(llvm::dbgs()
      << "Frame Enter: " << func.getName()
      << ", index " << frame << "\n"
  );
  for (unsigned i = 0, n = args.size(); i < n; ++i) {
    LLVM_DEBUG(llvm::dbgs() << "\t" << i << ":" << args[i] << "\n");
  }
  LLVM_DEBUG(llvm::dbgs() << "=======================================\n");
  #endif

  auto it = funcs_.emplace(&func, nullptr);
  if (it.second) {
    it.first->second = std::make_unique<SCCFunction>(func);
  }
  frames_.emplace_back(*it.first->second, frame, args);
  activeFrames_.push_back(frame);
  executedFunctions_.insert(&func);
  return frame;
}

// -----------------------------------------------------------------------------
unsigned SymbolicContext::EnterFrame(llvm::ArrayRef<Func::StackObject> objects)
{
  unsigned frame = frames_.size();
  frames_.emplace_back(frame, objects);
  activeFrames_.push_back(frame);
  return frame;
}


// -----------------------------------------------------------------------------
void SymbolicContext::LeaveFrame(Func &func)
{
  auto *frame = GetActiveFrame();
  assert(frame && "no frames left to pop from stack");
  assert(frame->GetFunc() == &func && "invalid frame");
  LLVM_DEBUG(llvm::dbgs()
      << "Frame Leave: " << func.getName()
      << ", index " << frame->GetIndex() << "\n"
  );
  frame->Leave();
  activeFrames_.pop_back();
}

// -----------------------------------------------------------------------------
bool SymbolicContext::HasFrame(Func &func)
{
  for (const auto &frame : frames_) {
    if (frame.IsValid() && frame.GetFunc() == &func) {
      return true;
    }
  }
  return false;
}

// -----------------------------------------------------------------------------
bool SymbolicContext::Store(
    const SymbolicPointer &addr,
    const SymbolicValue &val,
    Type type)
{
  LLVM_DEBUG(llvm::dbgs()
      << "Storing " << val << ":" << type << " to " << addr << "\n"
  );
  auto begin = addr.begin();
  if (std::next(begin) == addr.end()) {
    switch (begin->GetKind()) {
      case SymbolicAddress::Kind::ATOM: {
        auto &a = begin->AsAtom();
        auto &object = GetObject(*a.Symbol);
        return object.Store(a.Symbol, a.Offset, val, type);
      }
      case SymbolicAddress::Kind::ATOM_RANGE: {
        auto &a = begin->AsAtomRange();
        auto &object = GetObject(*a.Symbol);
        return object.StoreImprecise(val, type);
      }
      case SymbolicAddress::Kind::FRAME: {
        auto &a = begin->AsFrame();
        auto &object = GetFrame(a.Frame, a.Object);
        return object.Store(a.Offset, val, type);
      }
      case SymbolicAddress::Kind::FRAME_RANGE: {
        auto &a = begin->AsFrameRange();
        auto &object = GetFrame(a.Frame, a.Object);
        return object.StoreImprecise(val, type);
      }
      case SymbolicAddress::Kind::HEAP: {
        auto &a = begin->AsHeap();
        auto &object = GetHeap(a.Frame, *a.Alloc);
        return object.Store(a.Offset, val, type);
      }
      case SymbolicAddress::Kind::HEAP_RANGE: {
        auto &a = begin->AsHeapRange();
        auto &object = GetHeap(a.Frame, *a.Alloc);
        return object.StoreImprecise(val, type);
      }
      case SymbolicAddress::Kind::EXTERN: {
        llvm_unreachable("not implemented");
      }
      case SymbolicAddress::Kind::EXTERN_RANGE: {
        llvm_unreachable("not implemented");
      }
      case SymbolicAddress::Kind::FUNC: {
        llvm_unreachable("not implemented");
      }
      case SymbolicAddress::Kind::BLOCK: {
        llvm_unreachable("not implemented");
      }
      case SymbolicAddress::Kind::STACK: {
        llvm_unreachable("not implemented");
      }
    }
    llvm_unreachable("invalid address kind");
  } else {
    bool c = false;
    for (auto &address : addr) {
      switch (address.GetKind()) {
        case SymbolicAddress::Kind::ATOM: {
          auto &a = address.AsAtom();
          auto &object = GetObject(*a.Symbol);
          c = object.StoreImprecise(a.Symbol, a.Offset, val, type) || c;
          continue;
        }
        case SymbolicAddress::Kind::ATOM_RANGE: {
          auto &a = address.AsAtomRange();
          auto &object = GetObject(*a.Symbol);
          c = object.StoreImprecise(val, type) || c;
          continue;
        }
        case SymbolicAddress::Kind::FRAME: {
          auto &a = address.AsFrame();
          auto &object = GetFrame(a.Frame, a.Object);
          c = object.StoreImprecise(a.Offset, val, type) || c;
          continue;
        }
        case SymbolicAddress::Kind::FRAME_RANGE: {
          auto &a = address.AsFrameRange();
          auto &object = GetFrame(a.Frame, a.Object);
          c = object.StoreImprecise(val, type) || c;
          continue;
        }
        case SymbolicAddress::Kind::HEAP: {
          auto &a = address.AsHeap();
          auto &object = GetHeap(a.Frame, *a.Alloc);
          c = object.StoreImprecise(val, type) || c;
          continue;
        }
        case SymbolicAddress::Kind::HEAP_RANGE: {
          auto &a = address.AsHeapRange();
          auto &object = GetHeap(a.Frame, *a.Alloc);
          c = object.StoreImprecise(val, type) || c;
          continue;
        }
        case SymbolicAddress::Kind::EXTERN: {
          auto &a = address.AsExtern();
          c = StoreExtern(*a.Symbol, a.Offset, val, type) || c;
          continue;
        }
        case SymbolicAddress::Kind::EXTERN_RANGE: {
          auto &a = address.AsExternRange();
          c = StoreExtern(*a.Symbol, val, type) || c;
          continue;
        }
        case SymbolicAddress::Kind::FUNC:
        case SymbolicAddress::Kind::BLOCK:
        case SymbolicAddress::Kind::STACK: {
          continue;
        }
      }
      llvm_unreachable("invalid address kind");
    }
    return c;
  }
}

// -----------------------------------------------------------------------------
SymbolicValue SymbolicContext::Load(const SymbolicPointer &addr, Type type)
{
  LLVM_DEBUG(llvm::dbgs() << "Loading " << type << " from " << addr << "\n");

  std::optional<SymbolicValue> value;
  auto merge = [&value] (const SymbolicValue &v)
  {
    if (value) {
      value = value->LUB(v);
    } else {
      value = v;
    }
  };

  for (auto &address : addr) {
    switch (address.GetKind()) {
      case SymbolicAddress::Kind::ATOM: {
        auto &a = address.AsAtom();
        auto &object = GetObject(*a.Symbol);
        merge(object.Load(a.Symbol, a.Offset, type));
        continue;
      }
      case SymbolicAddress::Kind::ATOM_RANGE: {
        auto &a = address.AsAtom();
        auto &object = GetObject(*a.Symbol);
        merge(object.LoadImprecise(type));
        continue;
      }
      case SymbolicAddress::Kind::FRAME: {
        auto &a = address.AsFrame();
        auto &object = GetFrame(a.Frame, a.Object);
        merge(object.Load(a.Offset, type));
        continue;
      }
      case SymbolicAddress::Kind::FRAME_RANGE: {
        auto &a = address.AsFrame();
        auto &object = GetFrame(a.Frame, a.Object);
        merge(object.LoadImprecise(type));
        continue;
      }
      case SymbolicAddress::Kind::HEAP: {
        auto &a = address.AsHeap();
        auto &object = GetHeap(a.Frame, *a.Alloc);
        merge(object.Load(a.Offset, type));
        continue;
      }
      case SymbolicAddress::Kind::HEAP_RANGE: {
        auto &a = address.AsHeapRange();
        auto &object = GetHeap(a.Frame, *a.Alloc);
        merge(object.LoadImprecise(type));
        continue;
      }
      case SymbolicAddress::Kind::EXTERN: {
        auto &e = address.AsExtern();
        merge(LoadExtern(*e.Symbol, e.Offset, type));
        continue;
      }
      case SymbolicAddress::Kind::EXTERN_RANGE: {
        auto &e = address.AsExternRange();
        merge(LoadExtern(*e.Symbol, type));
        continue;
      }
      case SymbolicAddress::Kind::FUNC:
      case SymbolicAddress::Kind::BLOCK:
      case SymbolicAddress::Kind::STACK: {
        merge(SymbolicValue::Scalar());
        continue;
      }
    }
    llvm_unreachable("invalid address kind");
  }
  assert(value && "missing address");
  return *value;
}

// -----------------------------------------------------------------------------
static const char *kScalarExterns[] = {
    "_stext", "_etext",
    "_srodata", "_erodata",
    "_end",
    "caml__data_begin",
    "caml__data_end",
    "caml__code_begin",
    "caml__code_end",
    "caml_call_gc",
    "caml__frametable"
};

// -----------------------------------------------------------------------------
class PointerClosure final {
public:
  PointerClosure(SymbolicContext &ctx) : ctx_(ctx) {}

  void Queue(Global *global)
  {
    qg.push(global);
  }

  void Queue(unsigned frame, CallSite *alloc)
  {
    qh.emplace(frame, alloc);
  }

  void Queue(const SymbolicValue &value)
  {
    if (auto vptr = value.AsPointer()) {
      Queue(*vptr);
    }
  }

  void Queue(const SymbolicPointer &ptr)
  {
    for (auto &address : ptr) {
      switch (address.GetKind()) {
        case SymbolicAddress::Kind::ATOM: {
          qg.push(address.AsAtom().Symbol);
          continue;
        }
        case SymbolicAddress::Kind::ATOM_RANGE: {
          qg.push(address.AsAtomRange().Symbol);
          continue;
        }
        case SymbolicAddress::Kind::EXTERN: {
          qg.push(address.AsExtern().Symbol);
          continue;
        }
        case SymbolicAddress::Kind::EXTERN_RANGE: {
          qg.push(address.AsExternRange().Symbol);
          continue;
        }
        case SymbolicAddress::Kind::FRAME: {
          const auto &f = address.AsFrame();
          qf.emplace(f.Frame, f.Object);
          continue;
        }
        case SymbolicAddress::Kind::FRAME_RANGE: {
          const auto &f = address.AsFrameRange();
          qf.emplace(f.Frame, f.Object);
          continue;
        }
        case SymbolicAddress::Kind::HEAP: {
          const auto &a = address.AsHeap();
          qh.emplace(a.Frame, a.Alloc);
          continue;
        }
        case SymbolicAddress::Kind::HEAP_RANGE: {
          const auto &a = address.AsHeapRange();
          qh.emplace(a.Frame, a.Alloc);
          continue;
        }
        case SymbolicAddress::Kind::FUNC: {
          ptr_.Add(address.AsFunc().Fn);
          continue;
        }
        case SymbolicAddress::Kind::BLOCK: {
          ptr_.Add(address.AsBlock().B);
          continue;
        }
        case SymbolicAddress::Kind::STACK: {
          ptr_.Add(address.AsStack().Frame);
          continue;
        }
      }
      llvm_unreachable("invalid address kind");
    }
  }

  const SymbolicPointer &Closure()
  {
    while (!qg.empty() || !qo.empty() || !qf.empty() || !qh.empty()) {
      while (!qg.empty()) {
        auto &g = *qg.front();
        qg.pop();
        if (!vg.insert(&g).second) {
          continue;
        }
        switch (g.GetKind()) {
          case Global::Kind::ATOM: {
            qo.push(static_cast<Atom &>(g).getParent());
            continue;
          }
          case Global::Kind::FUNC: {
            ptr_.Add(&static_cast<Func &>(g));
            continue;
          }
          case Global::Kind::BLOCK: {
            ptr_.Add(&static_cast<Block &>(g));
            continue;
          }
          case Global::Kind::EXTERN: {
            LLVM_DEBUG(llvm::dbgs() << "Extern: " << g.getName() << "\n");

            bool scalar = false;
            unsigned n = sizeof(kScalarExterns) / sizeof(kScalarExterns[0]);
            for (size_t i = 0; i < n; ++i) {
              if (g.getName() == kScalarExterns[i]) {
                scalar = true;
                break;
              }
            }
            if (scalar) {
              continue;
            }
            llvm_unreachable("not implemented");
          }
        }
        llvm_unreachable("invalid global kind");
      }
      while (!qo.empty()) {
        auto &object = *qo.front();
        qo.pop();
        if (!vo.insert(&object).second) {
          continue;
        }
        for (Atom &atom : object) {
          ptr_.Add(&atom);
        }
        for (const auto &value : ctx_.GetObject(object)) {
          Queue(value);
        }
      }
      while (!qf.empty()) {
        auto [frame, object] = qf.front();
        qf.pop();
        if (!vf.insert({frame, object}).second) {
          continue;
        }
        ptr_.Add(frame, object);
        for (const auto &value : ctx_.GetFrame(frame, object)) {
          Queue(value);
        }
      }
      while (!qh.empty()) {
        auto [frame, site] = qh.front();
        qh.pop();
        if (!vh.emplace(frame, site).second) {
          continue;
        }
        ptr_.Add(frame, site);
        for (const auto &value : ctx_.GetHeap(frame, *site)) {
          Queue(value);
        }
      }
    }

    return ptr_;
  }

private:
  SymbolicContext &ctx_;
  SymbolicPointer ptr_;

  std::queue<Global *> qg;
  std::queue<Object *> qo;
  std::queue<std::pair<unsigned, unsigned>> qf;
  std::queue<std::pair<unsigned, CallSite *>> qh;

  std::set<Global *> vg;
  std::set<Object *> vo;
  std::set<std::pair<unsigned, CallSite *>> vh;
  std::set<std::pair<unsigned, unsigned>> vf;
};

// -----------------------------------------------------------------------------
SymbolicPointer SymbolicContext::Taint(
    const std::set<Global *> &globals,
    const std::set<std::pair<unsigned, unsigned>> &frames,
    const std::set<std::pair<unsigned, CallSite *>> &sites)
{
  PointerClosure closure(*this);
  for (auto *g : globals) {
    closure.Queue(g);
  }
  for (auto [frame, object] : frames) {
    llvm_unreachable("not implemented");
  }
  for (auto [frame, site] : sites) {
    closure.Queue(frame, site);
  }
  return closure.Closure();
}

// -----------------------------------------------------------------------------
SymbolicPointer SymbolicContext::Taint(const SymbolicPointer &ptr)
{
  PointerClosure closure(*this);
  closure.Queue(ptr);
  return closure.Closure();
}

// -----------------------------------------------------------------------------
SymbolicPointer SymbolicContext::Malloc(
    CallSite &site,
    std::optional<size_t> size)
{
  LLVM_DEBUG(llvm::dbgs() << "\t-----------------------\n");
  LLVM_DEBUG(llvm::dbgs()
      << "\tAllocation <"
      << &site << "> "
      << site.getParent()->getParent()->getName()
      << ":" << site.getParent()->getName()
      << "\n";
  );
  LLVM_DEBUG(llvm::dbgs() << "\t-----------------------\n");

  auto frame = GetActiveFrame()->GetIndex();
  auto key = std::make_pair(frame, &site);
  if (auto it = allocs_.find(key); it != allocs_.end()) {
    llvm_unreachable("not implemented");
  } else {
    allocs_.emplace(key, new SymbolicHeapObject(site, size));
  }
  return SymbolicPointer(frame, &site, 0);
}

// -----------------------------------------------------------------------------
bool SymbolicContext::StoreExtern(
    const Extern &e,
    const SymbolicValue &value,
    Type type)
{
  LLVM_DEBUG(llvm::dbgs() << "Store to extern: " << e.getName() << "\n");
  if (e.getName() == "_end" || e.getName() == "caml__frametable") {
    return false;
  }
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
bool SymbolicContext::StoreExtern(
    const Extern &e,
    int64_t off,
    const SymbolicValue &value,
    Type type)
{
  LLVM_DEBUG(llvm::dbgs()
      << "Store to extern: " << e.getName() << " + " << off << "\n"
  );
  if (e.getName() == "_end" || e.getName() == "caml__frametable") {
    return false;
  }
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
SymbolicValue SymbolicContext::LoadExtern(
    const Extern &e,
    int64_t offset,
    Type type)
{
  LLVM_DEBUG(llvm::dbgs() << "Load from extern: " << e.getName() << "\n");
  if (e.getName() == "caml__frametable") {
    if (offset == 0) {
      return SymbolicValue::LowerBoundedInteger(
          APInt(GetSize(type) * 8, 1, true)
      );
    } else {
      return SymbolicValue::Scalar();
    }
  }
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
SymbolicValue SymbolicContext::LoadExtern(
    const Extern &e,
    Type type)
{
  LLVM_DEBUG(llvm::dbgs() << "Imprecise load: " << e.getName() << "\n");
  // Over-approximate a store to an arbitrary external pointer.
  if (e.getName() == "caml__frametable" || e.getName() == "_end") {
    return SymbolicValue::Scalar();
  } else {
    llvm_unreachable("not implemented");
  }
}

// -----------------------------------------------------------------------------
SymbolicDataObject &SymbolicContext::GetObject(Atom &atom)
{
  return GetObject(*atom.getParent());
}

// -----------------------------------------------------------------------------
SymbolicDataObject &SymbolicContext::GetObject(Object &parent)
{
  auto it = objects_.emplace(&parent, nullptr);
  if (it.second) {
    it.first->second.reset(new SymbolicDataObject(parent));
  }
  return *it.first->second;
}

// -----------------------------------------------------------------------------
SymbolicHeapObject &SymbolicContext::GetHeap(unsigned frame, CallSite &site)
{
  auto it = allocs_.find({frame, &site});
  assert(it != allocs_.end() && "invalid heap object");
  return *allocs_[{frame, &site}];
}

// -----------------------------------------------------------------------------
void SymbolicContext::LUB(SymbolicContext &that)
{
  for (auto &[key, object] : that.objects_) {
    if (auto it = objects_.find(key); it != objects_.end()) {
      it->second->LUB(*object);
    } else {
      objects_.emplace(key, std::make_unique<SymbolicDataObject>(*object));
    }
  }

  for (unsigned i = 0, n = that.frames_.size(); i < n; ++i) {
    frames_[i].LUB(that.frames_[i]);
  }

  for (auto &[key, object] : that.allocs_) {
    if (auto it = allocs_.find(key); it != allocs_.end()) {
      it->second->LUB(*object);
    } else {
      allocs_.emplace(key, std::make_unique<SymbolicHeapObject>(*object));
    }
  }

  for (auto *func : that.executedFunctions_) {
    executedFunctions_.insert(func);
  }
}
