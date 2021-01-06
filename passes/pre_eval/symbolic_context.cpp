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
{
  for (auto &[g, object] : that.objects_) {
    objects_.emplace(g, std::make_unique<SymbolicDataObject>(*object));
  }
  for (auto &frame : that.frames_) {
    frames_.emplace_back(frame);
  }
}

// -----------------------------------------------------------------------------
SymbolicContext::~SymbolicContext()
{
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

  frames_.emplace_back(func, frame, args);
  activeFrames_.push(frame);
  return frame;
}

// -----------------------------------------------------------------------------
unsigned SymbolicContext::EnterFrame(llvm::ArrayRef<Func::StackObject> objects)
{
  unsigned frame = frames_.size();
  frames_.emplace_back(frame, objects);
  activeFrames_.push(frame);
  return frame;
}


// -----------------------------------------------------------------------------
void SymbolicContext::LeaveFrame(Func &func)
{
  auto &frame = GetActiveFrame();
  assert(frame.GetFunc() == &func && "invalid frame");
  LLVM_DEBUG(llvm::dbgs()
      << "Frame Leave: " << func.getName()
      << ", index " << frame.GetIndex() << "\n"
  );
  frame.Leave();
  activeFrames_.pop();
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
      case SymbolicAddress::Kind::GLOBAL: {
        auto &a = begin->AsGlobal();
        return StoreGlobal(a.Symbol, a.Offset, val, type);
      }
      case SymbolicAddress::Kind::GLOBAL_RANGE: {
        auto &a = begin->AsGlobalRange();
        return StoreGlobalImprecise(a.Symbol, val, type);
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
        case SymbolicAddress::Kind::GLOBAL: {
          auto &a = address.AsGlobal();
          c = StoreGlobalImprecise(a.Symbol, a.Offset, val, type) || c;
          continue;
        }
        case SymbolicAddress::Kind::GLOBAL_RANGE: {
          auto &a = address.AsGlobalRange();
          c = StoreGlobalImprecise(a.Symbol, val, type) || c;
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
      case SymbolicAddress::Kind::GLOBAL: {
        auto &a = address.AsGlobal();
        merge(LoadGlobal(a.Symbol, a.Offset, type));
        continue;
      }
      case SymbolicAddress::Kind::GLOBAL_RANGE: {
        merge(LoadGlobalImprecise(address.AsGlobal().Symbol, type));
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
        case SymbolicAddress::Kind::GLOBAL: {
          qg.push(address.AsGlobal().Symbol);
          continue;
        }
        case SymbolicAddress::Kind::GLOBAL_RANGE: {
          qg.push(address.AsGlobalRange().Symbol);
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
            static const char *kLimit[] = {
              "_stext", "_etext",
              "_srodata", "_erodata",
              "_end",
              "caml__data_begin", "caml__data_end",

              "caml_call_gc",
              "caml__frametable"
            };

            bool special = false;
            for (size_t i = 0, n = sizeof(kLimit) / sizeof(kLimit[0]); i < n; ++i) {
              if (g.getName() == kLimit[i]) {
                special = true;
                break;
              }
            }
            if (special) {
              continue;
            }
            llvm::errs() << g.getName() << "\n";
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

  auto frame = GetActiveFrame().GetIndex();
  auto key = std::make_pair(frame, &site);
  if (auto it = allocs_.find(key); it != allocs_.end()) {
    llvm_unreachable("not implemented");
  } else {
    allocs_.emplace(key, new SymbolicHeapObject(site, size));
  }
  return SymbolicPointer(frame, &site, 0);
}

// -----------------------------------------------------------------------------
bool SymbolicContext::StoreGlobal(
    Global *g,
    int64_t offset,
    const SymbolicValue &value,
    Type type)
{
  switch (g->GetKind()) {
    case Global::Kind::FUNC:
    case Global::Kind::BLOCK: {
      // Undefined behaviour - stores to these locations should not occur.
      return false;
    }
    case Global::Kind::EXTERN: {
      // Over-approximate a store to an arbitrary external pointer.
      return StoreExtern(static_cast<Extern &>(*g), value, type);
    }
    case Global::Kind::ATOM: {
      // Precise store to an atom.
      auto *atom = static_cast<Atom *>(g);
      auto &object = GetObject(*atom);
      return object.Store(atom, offset, value, type);
    }
  }
}

// -----------------------------------------------------------------------------
SymbolicValue SymbolicContext::LoadGlobal(Global *g, int64_t offset, Type type)
{
  switch (g->GetKind()) {
    case Global::Kind::FUNC:
    case Global::Kind::BLOCK: {
      // Undefined behaviour - stores to these locations should not occur.
      llvm_unreachable("not implemented");
    }
    case Global::Kind::EXTERN: {
      // Over-approximate a store to an arbitrary external pointer.
      return LoadExtern(static_cast<Extern &>(*g), type, offset);
    }
    case Global::Kind::ATOM: {
      // Precise store to an atom.
      auto *atom = static_cast<Atom *>(g);
      auto &object = GetObject(*atom);
      return object.Load(atom, offset, type);
    }
  }
}

// -----------------------------------------------------------------------------
bool SymbolicContext::StoreGlobalImprecise(
    Global *g,
    int64_t offset,
    const SymbolicValue &value,
    Type type)
{
  switch (g->GetKind()) {
    case Global::Kind::FUNC:
    case Global::Kind::BLOCK: {
      // Undefined behaviour - stores to these locations should not occur.
      return false;
    }
    case Global::Kind::EXTERN: {
      // Over-approximate a store to an arbitrary external pointer.
      return StoreExtern(static_cast<Extern &>(*g), value, type);
    }
    case Global::Kind::ATOM: {
      // Precise store to an atom.
      auto *atom = static_cast<Atom *>(g);
      auto &object = GetObject(*atom);
      return object.StoreImprecise(atom, offset, value, type);
    }
  }
  llvm_unreachable("invalid global kind");
}

// -----------------------------------------------------------------------------
SymbolicValue SymbolicContext::LoadGlobalImprecise(Global *g, Type type)
{
  switch (g->GetKind()) {
    case Global::Kind::FUNC:
    case Global::Kind::BLOCK: {
      // Undefined behaviour - stores to these locations should not occur.
      return SymbolicValue::Undefined();
    }
    case Global::Kind::EXTERN: {
      LLVM_DEBUG(llvm::dbgs() << "Imprecise load: " << g->getName() << "\n");
      // Over-approximate a store to an arbitrary external pointer.
      if (g->getName() == "caml__frametable" || g->getName() == "_end") {
        return SymbolicValue::Scalar();
      } else {
        llvm_unreachable("not implemented");
      }
    }
    case Global::Kind::ATOM: {
      auto &object = GetObject(*static_cast<Atom *>(g));
      return object.LoadImprecise(type);
    }
  }
  llvm_unreachable("invalid global kind");
}

// -----------------------------------------------------------------------------
bool SymbolicContext::StoreGlobalImprecise(
    Global *g,
    const SymbolicValue &value,
    Type type)
{
  switch (g->GetKind()) {
    case Global::Kind::FUNC:
    case Global::Kind::BLOCK: {
      // Undefined behaviour - stores to these locations should not occur.
      return false;
    }
    case Global::Kind::EXTERN: {
      // Over-approximate a store to an arbitrary external pointer.
      return StoreExtern(static_cast<Extern &>(*g), value, type);
    }
    case Global::Kind::ATOM: {
      // Precise store to an atom.
      auto &object = GetObject(*static_cast<Atom *>(g));
      return object.StoreImprecise(value, type);
    }
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
SymbolicValue SymbolicContext::LoadExtern(
    const Extern &e,
    Type type,
    int64_t offset)
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
    llvm_unreachable("not implemented");
  }
}
