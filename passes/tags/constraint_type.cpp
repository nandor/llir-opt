// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <stack>

#include "core/prog.h"
#include "passes/tags/constraints.h"
#include "passes/tags/tagged_type.h"
#include "passes/tags/register_analysis.h"

using namespace tags;



// -----------------------------------------------------------------------------
bool operator<(ConstraintType a, ConstraintType b)
{
  switch (a) {
    case ConstraintType::BOT: {
      return b != ConstraintType::BOT;
    }
    case ConstraintType::INT: {
      return b == ConstraintType::PTR_INT ||
             b == ConstraintType::ADDR_INT ||
             b == ConstraintType::HEAP_INT;
    }
    case ConstraintType::HEAP_INT: {
      return b == ConstraintType::PTR ||
             b == ConstraintType::PTR_INT ||
             b == ConstraintType::ADDR_INT;
    }
    case ConstraintType::HEAP: {
      return b == ConstraintType::HEAP_INT ||
             b == ConstraintType::PTR ||
             b == ConstraintType::PTR_INT ||
             b == ConstraintType::ADDR ||
             b == ConstraintType::ADDR_INT;
    }
    case ConstraintType::PTR_BOT: {
      return b == ConstraintType::HEAP ||
             b == ConstraintType::HEAP_INT ||
             b == ConstraintType::PTR ||
             b == ConstraintType::PTR_INT ||
             b == ConstraintType::ADDR ||
             b == ConstraintType::ADDR_INT ||
             b == ConstraintType::FUNC;
    }
    case ConstraintType::YOUNG: {
      return b == ConstraintType::HEAP ||
             b == ConstraintType::HEAP_INT ||
             b == ConstraintType::PTR ||
             b == ConstraintType::PTR_INT;
    }
    case ConstraintType::FUNC: {
      return b == ConstraintType::HEAP ||
             b == ConstraintType::HEAP_INT ||
             b == ConstraintType::PTR ||
             b == ConstraintType::PTR_INT;
    }
    case ConstraintType::PTR: {
      return b == ConstraintType::PTR_INT;
    }
    case ConstraintType::PTR_INT: {
      return false;
    }
    case ConstraintType::ADDR: {
      return b == ConstraintType::PTR ||
             b == ConstraintType::PTR_INT ||
             b == ConstraintType::ADDR_INT;
    }
    case ConstraintType::ADDR_INT: {
      return b == ConstraintType::PTR_INT;
    }
  }
  llvm_unreachable("invalid kind");
}

// -----------------------------------------------------------------------------
llvm::raw_ostream &operator<<(llvm::raw_ostream &os, ConstraintType type)
{
  switch (type) {
    case ConstraintType::BOT: os << "bot"; return os;
    case ConstraintType::INT: os << "int"; return os;
    case ConstraintType::PTR_BOT: os << "ptr_bot"; return os;
    case ConstraintType::YOUNG: os << "young"; return os;
    case ConstraintType::HEAP: os << "heap"; return os;
    case ConstraintType::ADDR: os << "addr"; return os;
    case ConstraintType::PTR: os << "ptr"; return os;
    case ConstraintType::ADDR_INT: os << "addr|int"; return os;
    case ConstraintType::PTR_INT: os << "ptr|int"; return os;
    case ConstraintType::HEAP_INT: os << "heap|int"; return os;
    case ConstraintType::FUNC: os << "func"; return os;
  }
  llvm_unreachable("invalid constraint kind");
}

// -----------------------------------------------------------------------------
ConstraintType tags::LUB(ConstraintType a, ConstraintType b)
{
  switch (a) {
    case ConstraintType::BOT: return b;
    case ConstraintType::INT: {
      switch (b) {
        case ConstraintType::BOT:       llvm_unreachable("not implemented");
        case ConstraintType::INT:       return ConstraintType::INT;
        case ConstraintType::HEAP_INT:  return ConstraintType::HEAP_INT;
        case ConstraintType::HEAP:      return ConstraintType::PTR_INT;
        case ConstraintType::PTR_BOT:   llvm_unreachable("not implemented");
        case ConstraintType::YOUNG:     llvm_unreachable("not implemented");
        case ConstraintType::FUNC:      return ConstraintType::PTR_INT;
        case ConstraintType::PTR:       return ConstraintType::PTR_INT;
        case ConstraintType::PTR_INT:   return ConstraintType::PTR_INT;
        case ConstraintType::ADDR:      llvm_unreachable("not implemented");
        case ConstraintType::ADDR_INT:  return ConstraintType::ADDR_INT;
      }
      llvm_unreachable("invalid kind");
    }
    case ConstraintType::HEAP_INT: {
      switch (b) {
        case ConstraintType::BOT:       return ConstraintType::HEAP;
        case ConstraintType::INT:       return ConstraintType::HEAP_INT;
        case ConstraintType::HEAP_INT:  return ConstraintType::HEAP_INT;
        case ConstraintType::HEAP:      return ConstraintType::HEAP_INT;
        case ConstraintType::PTR_BOT:   llvm_unreachable("not implemented");
        case ConstraintType::YOUNG:     llvm_unreachable("not implemented");
        case ConstraintType::FUNC:      llvm_unreachable("not implemented");
        case ConstraintType::PTR:       return ConstraintType::PTR_INT;
        case ConstraintType::PTR_INT:   return ConstraintType::PTR_INT;
        case ConstraintType::ADDR:      return ConstraintType::PTR_INT;
        case ConstraintType::ADDR_INT:  return ConstraintType::PTR_INT;
      }
      llvm_unreachable("invalid kind");
    }
    case ConstraintType::HEAP: {
      switch (b) {
        case ConstraintType::BOT:       return ConstraintType::HEAP;
        case ConstraintType::INT:       return ConstraintType::PTR_INT;
        case ConstraintType::HEAP_INT:  return ConstraintType::HEAP_INT;
        case ConstraintType::HEAP:      return ConstraintType::HEAP;
        case ConstraintType::PTR_BOT:   llvm_unreachable("not implemented");
        case ConstraintType::YOUNG:     return ConstraintType::HEAP;
        case ConstraintType::FUNC:      return ConstraintType::HEAP;
        case ConstraintType::PTR:       return ConstraintType::PTR;
        case ConstraintType::PTR_INT:   return ConstraintType::PTR_INT;
        case ConstraintType::ADDR:      return ConstraintType::PTR;
        case ConstraintType::ADDR_INT:  return ConstraintType::PTR_INT;
      }
      llvm_unreachable("invalid kind");
    }
    case ConstraintType::PTR_BOT: {
      llvm_unreachable("not implemented");
    }
    case ConstraintType::YOUNG: {
      switch (b) {
        case ConstraintType::BOT:       return ConstraintType::YOUNG;
        case ConstraintType::INT:       return ConstraintType::PTR_INT;
        case ConstraintType::HEAP_INT:  llvm_unreachable("not implemented");
        case ConstraintType::HEAP:      llvm_unreachable("not implemented");
        case ConstraintType::PTR_BOT:   llvm_unreachable("not implemented");
        case ConstraintType::YOUNG:     return ConstraintType::YOUNG;
        case ConstraintType::FUNC:      return ConstraintType::HEAP;
        case ConstraintType::PTR:       return ConstraintType::PTR;
        case ConstraintType::PTR_INT:   return ConstraintType::PTR_INT;
        case ConstraintType::ADDR:      llvm_unreachable("not implemented");
        case ConstraintType::ADDR_INT:  llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid kind");
    }
    case ConstraintType::FUNC: {
      switch (b) {
        case ConstraintType::BOT: llvm_unreachable("not implemented");
        case ConstraintType::INT: return ConstraintType::PTR_INT;
        case ConstraintType::HEAP_INT: llvm_unreachable("not implemented");
        case ConstraintType::HEAP: llvm_unreachable("not implemented");
        case ConstraintType::PTR_BOT: llvm_unreachable("not implemented");
        case ConstraintType::YOUNG: llvm_unreachable("not implemented");
        case ConstraintType::FUNC: return ConstraintType::FUNC;
        case ConstraintType::PTR: return ConstraintType::PTR;
        case ConstraintType::PTR_INT: return ConstraintType::PTR_INT;
        case ConstraintType::ADDR: llvm_unreachable("not implemented");
        case ConstraintType::ADDR_INT: llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid kind");
    }
    case ConstraintType::PTR: {
      switch (b) {
        case ConstraintType::BOT:      llvm_unreachable("not implemented");
        case ConstraintType::INT:      return ConstraintType::PTR_INT;
        case ConstraintType::HEAP_INT: return ConstraintType::PTR_INT;
        case ConstraintType::HEAP:     return ConstraintType::PTR;
        case ConstraintType::PTR_BOT:  llvm_unreachable("not implemented");
        case ConstraintType::YOUNG:    llvm_unreachable("not implemented");
        case ConstraintType::FUNC:     return ConstraintType::PTR;
        case ConstraintType::PTR:      return ConstraintType::PTR;
        case ConstraintType::PTR_INT:  return ConstraintType::PTR_INT;
        case ConstraintType::ADDR:     return ConstraintType::PTR;
        case ConstraintType::ADDR_INT: return ConstraintType::PTR_INT;
      }
      llvm_unreachable("invalid kind");
    }
    case ConstraintType::PTR_INT: return ConstraintType::PTR_INT;
    case ConstraintType::ADDR: {
      switch (b) {
        case ConstraintType::BOT:       llvm_unreachable("not implemented");
        case ConstraintType::INT:       llvm_unreachable("not implemented");
        case ConstraintType::HEAP_INT:  return ConstraintType::ADDR_INT;
        case ConstraintType::HEAP:      return ConstraintType::ADDR;
        case ConstraintType::PTR_BOT:   llvm_unreachable("not implemented");
        case ConstraintType::YOUNG:     llvm_unreachable("not implemented");
        case ConstraintType::FUNC:      llvm_unreachable("not implemented");
        case ConstraintType::PTR:       return ConstraintType::PTR;
        case ConstraintType::PTR_INT:   return ConstraintType::PTR_INT;
        case ConstraintType::ADDR:      return ConstraintType::ADDR;
        case ConstraintType::ADDR_INT:  return ConstraintType::ADDR_INT;
      }
      llvm_unreachable("invalid kind");
    }
    case ConstraintType::ADDR_INT: {
      switch (b) {
        case ConstraintType::BOT:       llvm_unreachable("not implemented");
        case ConstraintType::INT:       return ConstraintType::ADDR_INT;
        case ConstraintType::HEAP_INT:  return ConstraintType::PTR_INT;
        case ConstraintType::HEAP:      return ConstraintType::PTR_INT;
        case ConstraintType::PTR_BOT:   llvm_unreachable("not implemented");
        case ConstraintType::YOUNG:     return ConstraintType::PTR_INT;
        case ConstraintType::FUNC:      return ConstraintType::PTR_INT;
        case ConstraintType::PTR:       return ConstraintType::PTR_INT;
        case ConstraintType::PTR_INT:   return ConstraintType::PTR_INT;
        case ConstraintType::ADDR:      return ConstraintType::ADDR_INT;
        case ConstraintType::ADDR_INT:  return ConstraintType::ADDR_INT;
      }
      llvm_unreachable("invalid kind");
    }
  }
  llvm_unreachable("invalid kind");
}


// -----------------------------------------------------------------------------
ConstraintType tags::GLB(ConstraintType a, ConstraintType b)
{
  switch (a) {
    case ConstraintType::BOT: return ConstraintType::BOT;
    case ConstraintType::INT: {
      switch (b) {
        case ConstraintType::BOT:       return ConstraintType::BOT;
        case ConstraintType::INT:       return ConstraintType::INT;
        case ConstraintType::HEAP_INT:  llvm_unreachable("not implemented");
        case ConstraintType::HEAP:      return ConstraintType::BOT;
        case ConstraintType::PTR_BOT:   return ConstraintType::BOT;
        case ConstraintType::YOUNG: llvm_unreachable("not implemented");
        case ConstraintType::FUNC:      return ConstraintType::BOT;
        case ConstraintType::PTR:       return ConstraintType::BOT;
        case ConstraintType::PTR_INT: llvm_unreachable("not implemented");
        case ConstraintType::ADDR: llvm_unreachable("not implemented");
        case ConstraintType::ADDR_INT: llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid kind");
    }
    case ConstraintType::HEAP_INT: {
      llvm_unreachable("not implemented");
    }
    case ConstraintType::HEAP: {
      switch (b) {
        case ConstraintType::BOT:      return ConstraintType::BOT;
        case ConstraintType::INT:      return ConstraintType::BOT;
        case ConstraintType::HEAP_INT: llvm_unreachable("not implemented");
        case ConstraintType::HEAP:     return ConstraintType::HEAP;
        case ConstraintType::PTR_BOT:  return ConstraintType::PTR_BOT;
        case ConstraintType::YOUNG:    llvm_unreachable("not implemented");
        case ConstraintType::FUNC:     llvm_unreachable("not implemented");
        case ConstraintType::PTR:      return ConstraintType::HEAP;
        case ConstraintType::PTR_INT:  llvm_unreachable("not implemented");
        case ConstraintType::ADDR:     return ConstraintType::PTR_BOT;
        case ConstraintType::ADDR_INT: llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid kind");
    }
    case ConstraintType::PTR_BOT: {
      switch (b) {
        case ConstraintType::BOT:       return ConstraintType::BOT;
        case ConstraintType::INT:       return ConstraintType::BOT;
        case ConstraintType::HEAP_INT:  llvm_unreachable("not implemented");
        case ConstraintType::HEAP:      return ConstraintType::PTR_BOT;
        case ConstraintType::PTR_BOT:   return ConstraintType::PTR_BOT;
        case ConstraintType::YOUNG:     llvm_unreachable("not implemented");
        case ConstraintType::FUNC:      return ConstraintType::PTR_BOT;
        case ConstraintType::PTR:       return ConstraintType::PTR_BOT;
        case ConstraintType::PTR_INT:   llvm_unreachable("not implemented");
        case ConstraintType::ADDR:      return ConstraintType::PTR_BOT;
        case ConstraintType::ADDR_INT:  llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid kind");
    }
    case ConstraintType::YOUNG: {
      switch (b) {
        case ConstraintType::BOT:      return ConstraintType::BOT;
        case ConstraintType::INT:      return ConstraintType::PTR_INT;
        case ConstraintType::HEAP_INT: llvm_unreachable("not implemented");
        case ConstraintType::HEAP:     llvm_unreachable("not implemented");
        case ConstraintType::PTR_BOT:  llvm_unreachable("not implemented");
        case ConstraintType::YOUNG:    return ConstraintType::YOUNG;
        case ConstraintType::FUNC:     return ConstraintType::PTR_BOT;
        case ConstraintType::PTR:      return ConstraintType::PTR_BOT;
        case ConstraintType::PTR_INT:  return ConstraintType::BOT;
        case ConstraintType::ADDR:     llvm_unreachable("not implemented");
        case ConstraintType::ADDR_INT: llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid kind");
    }
    case ConstraintType::FUNC: {
      switch (b) {
        case ConstraintType::BOT:      return ConstraintType::BOT;
        case ConstraintType::INT:      return ConstraintType::BOT;
        case ConstraintType::HEAP_INT: llvm_unreachable("not implemented");
        case ConstraintType::HEAP:     llvm_unreachable("not implemented");
        case ConstraintType::PTR_BOT:  return ConstraintType::PTR_BOT;
        case ConstraintType::YOUNG:    llvm_unreachable("not implemented");
        case ConstraintType::FUNC:     return ConstraintType::FUNC;
        case ConstraintType::PTR:      return ConstraintType::PTR_BOT;
        case ConstraintType::PTR_INT:  return ConstraintType::BOT;
        case ConstraintType::ADDR:     llvm_unreachable("not implemented");
        case ConstraintType::ADDR_INT: llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid kind");
    }
    case ConstraintType::PTR: {
      switch (b) {
        case ConstraintType::BOT: return ConstraintType::BOT;
        case ConstraintType::INT: return ConstraintType::BOT;
        case ConstraintType::HEAP_INT: llvm_unreachable("not implemented");
        case ConstraintType::HEAP:    return ConstraintType::PTR_BOT;
        case ConstraintType::PTR_BOT: return ConstraintType::PTR_BOT;
        case ConstraintType::YOUNG: llvm_unreachable("not implemented");
        case ConstraintType::FUNC: llvm_unreachable("not implemented");
        case ConstraintType::PTR:     return ConstraintType::PTR;
        case ConstraintType::PTR_INT: llvm_unreachable("not implemented");
        case ConstraintType::ADDR:    return ConstraintType::PTR_BOT;
        case ConstraintType::ADDR_INT: llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid kind");
    }
    case ConstraintType::PTR_INT: return a;
    case ConstraintType::ADDR: {
      switch (b) {
        case ConstraintType::BOT:       return ConstraintType::BOT;
        case ConstraintType::INT:       return ConstraintType::BOT;
        case ConstraintType::HEAP_INT:  llvm_unreachable("not implemented");
        case ConstraintType::HEAP:      return ConstraintType::HEAP;
        case ConstraintType::PTR_BOT:   return ConstraintType::PTR_BOT;
        case ConstraintType::YOUNG:     llvm_unreachable("not implemented");
        case ConstraintType::FUNC:      llvm_unreachable("not implemented");
        case ConstraintType::PTR:       return ConstraintType::PTR_BOT;
        case ConstraintType::PTR_INT:   llvm_unreachable("not implemented");
        case ConstraintType::ADDR:      return ConstraintType::ADDR;
        case ConstraintType::ADDR_INT:  llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid kind");
    }
    case ConstraintType::ADDR_INT: {
      llvm_unreachable("not implemented");
    }
  }
  llvm_unreachable("invalid kind");
}
