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
#include "core/data.h"
#include "passes/pre_eval/symbolic_value.h"
#include "passes/pre_eval/symbolic_context.h"
#include "passes/pre_eval/symbolic_heap.h"

#define DEBUG_TYPE "pre-eval"



// -----------------------------------------------------------------------------
SymbolicContext::SymbolicContext(const SymbolicContext &that)
  : heap_(that.heap_)
  , state_(that.state_)
  , funcs_(that.funcs_)
  , activeFrames_(that.activeFrames_)
  , extern_(that.extern_)
{
  for (auto &[id, object] : that.objects_) {
    objects_.emplace(id, std::make_unique<SymbolicObject>(*object));
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
SymbolicFrame *SymbolicContext::GetActiveFrame()
{
  if (activeFrames_.empty()) {
    return nullptr;
  } else {
    return &frames_[*activeFrames_.rbegin()];
  }
}

// -----------------------------------------------------------------------------
DAGFunc &SymbolicContext::GetSCCFunc(Func &func)
{
  auto it = funcs_.emplace(&func, nullptr);
  if (it.second) {
    it.first->second = std::make_unique<DAGFunc>(func);
  }
  return *it.first->second;
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

  std::vector<ID<SymbolicObject>> ids;
  for (auto &object : func.objects()) {
    auto id = heap_.Frame(frame, object.Index);
    ids.push_back(id);
    LLVM_DEBUG(llvm::dbgs() << "\nBuilding frame object " << id << "\n");
    objects_.emplace(id, new SymbolicObject(
        id,
        object.Size,
        object.Alignment,
        false,
        true
    ));
  }

  frames_.emplace_back(state_, GetSCCFunc(func), frame, args, ids);
  activeFrames_.push_back(frame);
  return frame;
}

// -----------------------------------------------------------------------------
unsigned SymbolicContext::EnterFrame(
    llvm::ArrayRef<std::optional<unsigned>> objects)
{
  unsigned frame = frames_.size();

  #ifndef NDEBUG
  LLVM_DEBUG(llvm::dbgs() << "=======================================\n");
  LLVM_DEBUG(llvm::dbgs() << "Root Frame: " << frame << "\n");
  LLVM_DEBUG(llvm::dbgs() << "=======================================\n");
  #endif

  std::vector<ID<SymbolicObject>> ids;
  for (unsigned i = 0, n = objects.size(); i < n; ++i) {
    auto id = heap_.Frame(frame, i);
    ids.push_back(id);
    LLVM_DEBUG(llvm::dbgs() << "\nBuilding frame object " << id << "\n");
    objects_.emplace(id, new SymbolicObject(
        id,
        objects[i],
        llvm::Align(8),
        false,
        false
    ));
  }

  frames_.emplace_back(state_, frame, ids);
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
void SymbolicContext::LeaveRoot()
{
  auto *frame = GetActiveFrame();
  assert(!frame->GetFunc() && "not a root frame");
  LLVM_DEBUG(llvm::dbgs() << "Leaving root frame\n");
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
void SymbolicContext::Taint(
    const SymbolicValue &taint,
    const SymbolicValue &tainted)
{
  // TODO
}

// -----------------------------------------------------------------------------
SymbolicObject *SymbolicContext::BuildObject(
    ID<SymbolicObject> id,
    Object *object)
{
  auto align = object->begin()->GetAlignment().value_or(llvm::Align(1));
  if (object->size() == 1) {
    Atom &atom = *object->begin();
    bool rdonly = object->getParent()->IsConstant();
    auto *obj = new SymbolicObject(id, atom.GetByteSize(), align, rdonly, true);
    unsigned off = 0;
    for (auto &item : atom) {
      switch (item.GetKind()) {
        case Item::Kind::INT8: {
          obj->Init(
              off,
              SymbolicValue::Integer(APInt(8, item.GetInt8(), true)),
              Type::I8
          );
          off += 1;
          continue;
        }
        case Item::Kind::INT16: {
          obj->Init(
              off,
              SymbolicValue::Integer(APInt(16, item.GetInt16(), true)),
              Type::I16
          );
          off += 2;
          continue;
        }
        case Item::Kind::INT32: {
          obj->Init(
              off,
              SymbolicValue::Integer(APInt(32, item.GetInt32(), true)),
              Type::I32
          );
          off += 4;
          continue;
        }
        case Item::Kind::INT64: {
          obj->Init(
              off,
              SymbolicValue::Integer(APInt(64, item.GetInt64(), true)),
              Type::I64
          );
          off += 8;
          continue;
        }
        case Item::Kind::EXPR32: {
          llvm_unreachable("not implemented");
        }
        case Item::Kind::EXPR64: {
          switch (item.GetExpr()->GetKind()) {
            case Expr::Kind::SYMBOL_OFFSET: {
              auto se = ::cast_or_null<SymbolOffsetExpr>(item.GetExpr());
              auto *g = se->GetSymbol();
              switch (g->GetKind()) {
                case Global::Kind::ATOM: {
                  obj->Init(
                      off,
                      SymbolicValue::Pointer(Pointer(
                          static_cast<Atom &>(*g),
                          se->GetOffset()
                      )),
                      Type::I64
                  );
                  off += 8;
                  continue;
                }
                case Global::Kind::EXTERN: {
                  llvm_unreachable("not implemented");
                }
                case Global::Kind::FUNC: {
                  assert(se->GetOffset() == 0 && "invalid offset");
                  obj->Init(
                      off,
                      SymbolicValue::Pointer(std::make_shared<SymbolicPointer>(
                          heap_.Function(static_cast<Func *>(g))
                      )),
                      Type::I64
                  );
                  off += 8;
                  continue;
                }
                case Global::Kind::BLOCK: {
                  llvm_unreachable("not implemented");
                }
              }
              llvm_unreachable("invalid global kind");
            }
          }
          llvm_unreachable("invalid expression kind");
        }
        case Item::Kind::FLOAT64: {
          llvm_unreachable("not implemented");
        }
        case Item::Kind::SPACE: {
          unsigned n = item.GetSpace();
          while (n >= 8) {
            obj->Init(
                off,
                SymbolicValue::Integer(APInt(64, 0, true)),
                Type::I64
            );
            n -= 8;
            off += 8;
          }
          if (n) {
            for (unsigned i = 0; i < n; ++i) {
              obj->Init(
                  off++,
                  SymbolicValue::Integer(APInt(8, 0, true)),
                  Type::I8
              );
            }
          }
          continue;
        }
        case Item::Kind::STRING: {
          for (char chr : item.GetString()) {
            obj->Init(
                off++,
                SymbolicValue::Integer(APInt(8, chr, true)),
                Type::I8
            );
          }
          continue;
        }
      }
      llvm_unreachable("invalid item kind");
    }
    #ifndef NDEBUG
    {
      LLVM_DEBUG(llvm::dbgs()
          << "\n------\n"
          << "Built object <" << id << ">:\n"
          << *object
          << "\n------\n");
      unsigned i = 0;
      for (auto &bucket : *obj) {
        LLVM_DEBUG(llvm::dbgs() << "\t" << i << ": " << bucket << "\n");
        i += 8;
      }
    }
    #endif
    return obj;
  } else {
    llvm_unreachable("not implemented");
  }
}

// -----------------------------------------------------------------------------
SymbolicPointer::Ref SymbolicContext::Pointer(Atom &atom, int64_t offset)
{
  auto *object = atom.getParent();
  auto id = heap_.Data(object);
  auto it = objects_.emplace(id, nullptr);
  if (it.second) {
    it.first->second.reset(BuildObject(id, object));
  }
  if (object->size() == 1) {
    return std::make_shared<SymbolicPointer>(id, offset);
  } else {
    llvm_unreachable("not implemented");
  }
}

// -----------------------------------------------------------------------------
SymbolicPointer::Ref
SymbolicContext::Pointer(unsigned frame,unsigned object, int64_t offset)
{
  return std::make_shared<SymbolicPointer>(heap_.Frame(frame, object), offset);
}

// -----------------------------------------------------------------------------
SymbolicObject &SymbolicContext::GetObject(ID<SymbolicObject> id)
{
  auto it = objects_.find(id);
  assert(it != objects_.end() && "object not in context");
  return *it->second;
}

// -----------------------------------------------------------------------------
SymbolicObject &SymbolicContext::GetObject(Object *object)
{
  auto id = heap_.Data(object);
  auto it = objects_.emplace(id, nullptr);
  if (it.second) {
    it.first->second.reset(BuildObject(id, object));
  }
  return *it.first->second;
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
      case SymbolicAddress::Kind::OBJECT: {
        auto &a = begin->AsObject();
        return GetObject(a.Object).Store(a.Offset, val, type);
      }
      case SymbolicAddress::Kind::OBJECT_RANGE: {
        auto &a = begin->AsObjectRange();
        return GetObject(a.Object).StoreImprecise(val, type);
      }
      case SymbolicAddress::Kind::EXTERN: {
        llvm_unreachable("not implemented");
      }
      case SymbolicAddress::Kind::EXTERN_RANGE: {
        auto &e = begin->AsExternRange();
        return StoreExtern(*e.Symbol, val, type);
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
        case SymbolicAddress::Kind::OBJECT: {
          auto &a = address.AsObject();
          c = GetObject(a.Object).StoreImprecise(a.Offset, val, type) || c;
          continue;
        }
        case SymbolicAddress::Kind::OBJECT_RANGE: {
          auto &a = address.AsObjectRange();
          c = GetObject(a.Object).StoreImprecise(val, type) || c;
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
      value->Merge(v);
    } else {
      value = v;
    }
  };

  for (auto &address : addr) {
    switch (address.GetKind()) {
      case SymbolicAddress::Kind::OBJECT: {
        auto &a = address.AsObject();
        merge(GetObject(a.Object).Load(a.Offset, type));
        continue;
      }
      case SymbolicAddress::Kind::OBJECT_RANGE: {
        auto &a = address.AsObjectRange();
        merge(GetObject(a.Object).LoadImprecise(type));
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
SymbolicPointer::Ref
SymbolicContext::Malloc(CallSite &site, std::optional<unsigned> size)
{
  auto frame = GetActiveFrame()->GetIndex();
  auto id = heap_.Alloc(frame, &site);

  LLVM_DEBUG(llvm::dbgs() << "\t-----------------------\n");
  LLVM_DEBUG(llvm::dbgs()
      << "\tAllocation <" << id << "> "
      << site.getParent()->getParent()->getName()
      << ":" << site.getParent()->getName()
      << "\n";
  );
  LLVM_DEBUG(llvm::dbgs() << "\t-----------------------\n");

  if (auto it = objects_.find(id); it != objects_.end()) {
    llvm_unreachable("not implemented");
  } else {
    objects_.emplace(id, new SymbolicObject(
        id,
        size,
        llvm::Align(8),
        false,
        true
    ));
  }
  return std::make_shared<SymbolicPointer>(id, 0);
}

// -----------------------------------------------------------------------------
bool SymbolicContext::StoreExtern(
    const Extern &e,
    const SymbolicValue &value,
    Type type)
{
  LLVM_DEBUG(llvm::dbgs() << "Store to extern: " << e.getName() << "\n");
  // Over-approximate a store to an extern.
  if (e.getName() == "_end") {
    return false;
  }
  if (e.getName() == "caml__data_begin" || e.getName() == "caml__data_end") {
    return false;
  }
  if (e.getName() == "caml__code_begin" || e.getName() == "caml__code_end") {
    return false;
  }
  if (e.getName() == "caml__frametable") {
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
  LLVM_DEBUG(llvm::dbgs() << "Store to extern: " << e.getName() << "\n");
  // Store to a precise location in an extern.
  if (e.getName() == "_end") {
    return false;
  }
  if (e.getName() == "_etext" || e.getName() == "_stext") {
    return false;
  }
  if (e.getName() == "_erodata" || e.getName() == "_srodata") {
    return false;
  }
  if (e.getName() == "caml__frametable") {
    return false;
  }
  if (e.getName() == "caml__code_begin" || e.getName() == "caml__code_end") {
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
  // Over-approximate a load from an arbitrary external pointer.
  if (e.getName() == "caml__frametable") {
    if (offset == 0) {
      return SymbolicValue::LowerBoundedInteger(
          APInt(GetSize(type) * 8, 1, true)
      );
    } else {
      return SymbolicValue::Scalar();
    }
  }
  if (e.getName() == "caml__code_begin") {
    return SymbolicValue::Scalar();
  }
  if (e.getName() == "caml__code_end") {
    return SymbolicValue::Scalar();
  }
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
SymbolicValue SymbolicContext::LoadExtern(
    const Extern &e,
    Type type)
{
  LLVM_DEBUG(llvm::dbgs() << "Imprecise load: " << e.getName() << "\n");
  // Over-approximate a load from an arbitrary external pointer.
  if (e.getName() == "_end") {
    return SymbolicValue::Scalar();
  }
  if (e.getName() == "caml__frametable") {
    return SymbolicValue::Scalar();
  }
  if (e.getName() == "caml__data_begin") {
    return SymbolicValue::Scalar();
  }
  if (e.getName() == "caml__code_begin") {
    return SymbolicValue::Scalar();
  }
  if (e.getName() == "caml__code_end") {
    return SymbolicValue::Scalar();
  }
  llvm_unreachable("not implemented");
}

// -----------------------------------------------------------------------------
void SymbolicContext::Merge(const SymbolicContext &that)
{
  for (auto &[key, object] : that.objects_) {
    if (auto it = objects_.find(key); it != objects_.end()) {
      it->second->Merge(*object);
    } else {
      objects_.emplace(key, std::make_unique<SymbolicObject>(*object));
    }
  }

  for (unsigned i = 0, n = that.frames_.size(); i < n; ++i) {
    if (i < frames_.size()) {
      frames_[i].Merge(that.frames_[i]);
    } else {
      frames_.emplace_back(that.frames_[i]);
    }
  }

  if (that.extern_) {
    if (extern_) {
      extern_->Merge(*that.extern_);
    } else {
      extern_ = that.extern_;
    }
  }
}

// -----------------------------------------------------------------------------
std::set<SymbolicFrame *> SymbolicContext::GetFrames(Func &func)
{
  std::set<SymbolicFrame *> frs;
  for (auto &frame : frames_) {
    if (frame.GetFunc() == &func) {
      frs.insert(&frame);
    }
  }
  return frs;
}
