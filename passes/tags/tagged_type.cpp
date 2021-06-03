// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "passes/tags/tagged_type.h"

using namespace tags;



// -----------------------------------------------------------------------------
TaggedType::TaggedType(const TaggedType &that)
  : k_(that.k_)
{
  switch (that.k_) {
    case Kind::CONST: {
      u_.IntVal.~APInt();
      return;
    }
    case Kind::MOD: {
      u_.ModVal = that.u_.ModVal;
      return;
    }
    case Kind::INT:
    case Kind::ZERO:
    case Kind::ONE:
    case Kind::ZERO_ONE:
    case Kind::UNKNOWN:
    case Kind::YOUNG:
    case Kind::HEAP:
    case Kind::VAL:
    case Kind::PTR:
    case Kind::PTR_NULL:
    case Kind::PTR_INT:
    case Kind::ADDR:
    case Kind::TAG_PTR:
    case Kind::UNDEF: {
      return;
    }
  }
  llvm_unreachable("invalid kind");
}

// -----------------------------------------------------------------------------
TaggedType::~TaggedType()
{
  Destroy();
}

// -----------------------------------------------------------------------------
void TaggedType::Destroy()
{
  switch (k_) {
    case Kind::CONST: {
      u_.IntVal.~APInt();
      return;
    }
    case Kind::INT:
    case Kind::MOD:
    case Kind::ZERO:
    case Kind::ONE:
    case Kind::ZERO_ONE:
    case Kind::UNKNOWN:
    case Kind::YOUNG:
    case Kind::HEAP:
    case Kind::VAL:
    case Kind::PTR:
    case Kind::PTR_NULL:
    case Kind::PTR_INT:
    case Kind::ADDR:
    case Kind::TAG_PTR:
    case Kind::UNDEF: {
      return;
    }
  }
  llvm_unreachable("invalid kind");
}

// -----------------------------------------------------------------------------
TaggedType &TaggedType::operator=(const TaggedType &that)
{
  Destroy();

  k_ = that.k_;

  switch (k_) {
    case Kind::CONST: {
      u_.IntVal.~APInt();
      break;
    }
    case Kind::MOD: {
      u_.ModVal = that.u_.ModVal;
      break;
    }
    case Kind::INT:
    case Kind::ZERO:
    case Kind::ONE:
    case Kind::ZERO_ONE:
    case Kind::UNKNOWN:
    case Kind::YOUNG:
    case Kind::HEAP:
    case Kind::VAL:
    case Kind::PTR:
    case Kind::PTR_NULL:
    case Kind::PTR_INT:
    case Kind::ADDR:
    case Kind::TAG_PTR:
    case Kind::UNDEF: {
      break;
    }
  }

  return *this;
}


// -----------------------------------------------------------------------------
bool TaggedType::IsEven() const
{
  switch (k_) {
    case Kind::ZERO:
    case Kind::YOUNG:
    case Kind::TAG_PTR: {
      return true;
    }
    case Kind::ONE:
    case Kind::ZERO_ONE:
    case Kind::INT: {
      return false;
    }
    case Kind::CONST: {
      llvm_unreachable("not implemented");
    }
    case Kind::MOD: {
      return u_.ModVal.Div % 2 == 0 && u_.ModVal.Rem % 2 == 0;
    }
    case Kind::UNKNOWN:
    case Kind::HEAP:
    case Kind::VAL:
    case Kind::PTR:
    case Kind::PTR_NULL:
    case Kind::PTR_INT:
    case Kind::ADDR:
    case Kind::UNDEF: {
      return false;
    }
  }
  llvm_unreachable("invalid kind");
}

// -----------------------------------------------------------------------------
bool TaggedType::IsOdd() const
{
  switch (k_) {
    case Kind::ONE: {
      return true;
    }
    case Kind::ZERO:
    case Kind::YOUNG:
    case Kind::TAG_PTR:
    case Kind::ZERO_ONE:
    case Kind::INT: {
      return false;
    }
    case Kind::CONST: {
      llvm_unreachable("not implemented");
    }
    case Kind::MOD: {
      return u_.ModVal.Div % 2 == 0 && u_.ModVal.Rem % 2 == 1;
    }
    case Kind::UNKNOWN:
    case Kind::HEAP:
    case Kind::VAL:
    case Kind::PTR:
    case Kind::PTR_NULL:
    case Kind::PTR_INT:
    case Kind::ADDR:
    case Kind::UNDEF: {
      return false;
    }
  }
  llvm_unreachable("invalid kind");
}

// -----------------------------------------------------------------------------
bool TaggedType::IsIntLike() const
{
  switch (k_) {
    case Kind::ZERO:
    case Kind::ONE:
    case Kind::ZERO_ONE:
    case Kind::MOD:
    case Kind::CONST:
    case Kind::INT: {
      return true;
    }
    case Kind::UNKNOWN:
    case Kind::YOUNG:
    case Kind::HEAP:
    case Kind::VAL:
    case Kind::PTR:
    case Kind::PTR_NULL:
    case Kind::PTR_INT:
    case Kind::ADDR:
    case Kind::TAG_PTR:
    case Kind::UNDEF: {
      return false;
    }
  }
  llvm_unreachable("invalid kind");
}

// -----------------------------------------------------------------------------
TaggedType TaggedType::operator|(const TaggedType &that) const
{
  switch (k_) {
    case Kind::UNKNOWN: return that;
    case Kind::UNDEF:   return that;
    case Kind::MOD: {
      switch (that.k_) {
        case Kind::UNDEF:    return *this;
        case Kind::UNKNOWN:  return *this;
        case Kind::ZERO:     return u_.ModVal.Rem == 0 ? *this : TaggedType::Int();
        case Kind::ONE:      return u_.ModVal.Rem == 1 ? *this : TaggedType::Int();
        case Kind::ZERO_ONE: return TaggedType::Int();
        case Kind::INT:      return TaggedType::Int();
        case Kind::PTR:      return TaggedType::PtrInt();
        case Kind::PTR_INT:  return TaggedType::PtrInt();
        case Kind::CONST:    llvm_unreachable("not implemented");
        case Kind::PTR_NULL: return TaggedType::PtrInt();
        case Kind::TAG_PTR:  return TaggedType::PtrInt();
        case Kind::ADDR:     return TaggedType::PtrInt();
        case Kind::MOD: {
          if (u_.ModVal.Div % that.u_.ModVal.Div) {
            unsigned rem = u_.ModVal.Rem % that.u_.ModVal.Div;
            if (rem == that.u_.ModVal.Rem) {
              return TaggedType::Modulo({that.u_.ModVal.Div, rem});
            } else {
              return TaggedType::Int();
            }
          }
          if (that.u_.ModVal.Div % u_.ModVal.Div) {
            unsigned rem = that.u_.ModVal.Rem % u_.ModVal.Div;
            if (rem == u_.ModVal.Rem) {
              return TaggedType::Modulo({u_.ModVal.Div, rem});
            } else {
              return TaggedType::Int();
            }
          }
          return TaggedType::Int();
        }
        case Kind::VAL:
        case Kind::HEAP:
        case Kind::YOUNG: {
          return IsOdd() ? TaggedType::Val() : TaggedType::PtrInt();
        }
      }
      llvm_unreachable("invalid kind");
    }
    case Kind::ONE: {
      switch (that.k_) {
        case Kind::UNKNOWN:  return TaggedType::One();
        case Kind::ONE:      return TaggedType::One();
        case Kind::INT:      return TaggedType::Int();
        case Kind::ZERO:     return TaggedType::ZeroOne();
        case Kind::ZERO_ONE: return TaggedType::ZeroOne();
        case Kind::VAL:      return TaggedType::Val();
        case Kind::HEAP:     return TaggedType::Val();
        case Kind::YOUNG:    return TaggedType::Val();
        case Kind::PTR:      return TaggedType::PtrInt();
        case Kind::UNDEF:    return TaggedType::One();
        case Kind::PTR_INT:  return TaggedType::PtrInt();
        case Kind::MOD:      return that.GetMod().Rem == 1 ? that : TaggedType::Int();
        case Kind::PTR_NULL: return TaggedType::PtrInt();
        case Kind::TAG_PTR:  return TaggedType::Ptr();
        case Kind::ADDR:     return TaggedType::Addr();
        case Kind::CONST:    llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid kind");
    }
    case Kind::ZERO: {
      switch (that.k_) {
        case Kind::UNKNOWN:  return *this;
        case Kind::UNDEF:    return *this;
        case Kind::ZERO:     return TaggedType::Zero();
        case Kind::ONE:      return TaggedType::ZeroOne();
        case Kind::ZERO_ONE: return TaggedType::ZeroOne();
        case Kind::HEAP:     return TaggedType::PtrNull();
        case Kind::MOD:      return that.GetMod().Rem == 0 ? that : TaggedType::Int();
        case Kind::INT:      return TaggedType::Int();
        case Kind::PTR_INT:  return TaggedType::PtrInt();
        case Kind::YOUNG:    return TaggedType::PtrNull();
        case Kind::VAL:      return TaggedType::PtrInt();
        case Kind::PTR_NULL: return TaggedType::PtrNull();
        case Kind::PTR:      return TaggedType::PtrNull();
        case Kind::TAG_PTR:  llvm_unreachable("not implemented");
        case Kind::ADDR:     llvm_unreachable("not implemented");
        case Kind::CONST:    llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid kind");
    }
    case Kind::ZERO_ONE: {
      switch (that.k_) {
        case Kind::UNKNOWN:  return TaggedType::ZeroOne();
        case Kind::UNDEF:    return TaggedType::ZeroOne();
        case Kind::ZERO:     return TaggedType::ZeroOne();
        case Kind::ONE:      return TaggedType::ZeroOne();
        case Kind::ZERO_ONE: return TaggedType::ZeroOne();
        case Kind::INT:      return TaggedType::Int();
        case Kind::MOD:      return TaggedType::Int();
        case Kind::VAL:      llvm_unreachable("not implemented");
        case Kind::HEAP:     llvm_unreachable("not implemented");
        case Kind::PTR:      return TaggedType::PtrInt();
        case Kind::YOUNG:    return TaggedType::PtrInt();
        case Kind::PTR_INT:  return TaggedType::PtrInt();
        case Kind::PTR_NULL: return TaggedType::PtrInt();
        case Kind::TAG_PTR:  llvm_unreachable("not implemented");
        case Kind::ADDR:     llvm_unreachable("not implemented");
        case Kind::CONST:    llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid kind");
    }
    case Kind::CONST: {
      llvm_unreachable("not implemented");
    }
    case Kind::INT: {
      switch (that.k_) {
        case Kind::UNKNOWN:  return TaggedType::Int();
        case Kind::CONST:    return TaggedType::Int();
        case Kind::MOD:      return TaggedType::Int();
        case Kind::ZERO:     return TaggedType::Int();
        case Kind::ONE:      return TaggedType::Int();
        case Kind::ZERO_ONE: return TaggedType::Int();
        case Kind::INT:      return TaggedType::Int();
        case Kind::UNDEF:    return TaggedType::Int();
        case Kind::VAL:      return TaggedType::PtrInt();
        case Kind::HEAP:     return TaggedType::PtrInt();
        case Kind::YOUNG:    return TaggedType::PtrInt();
        case Kind::PTR:      return TaggedType::PtrInt();
        case Kind::PTR_NULL: return TaggedType::PtrInt();
        case Kind::PTR_INT:  return TaggedType::PtrInt();
        case Kind::TAG_PTR:  return TaggedType::PtrInt();
        case Kind::ADDR:     return TaggedType::PtrInt();
      }
      llvm_unreachable("invalid kind");
    }
    case Kind::VAL: {
      switch (that.k_) {
        case Kind::UNKNOWN:   return TaggedType::Val();
        case Kind::ONE:       return TaggedType::Val();
        case Kind::VAL:       return TaggedType::Val();
        case Kind::ZERO:      return TaggedType::Val();
        case Kind::HEAP:      return TaggedType::Val();
        case Kind::INT:       return TaggedType::Val();
        case Kind::UNDEF:     return TaggedType::Val();
        case Kind::YOUNG:     return TaggedType::Val();
        case Kind::ZERO_ONE:  return TaggedType::PtrInt();
        case Kind::PTR:       return TaggedType::PtrInt();
        case Kind::PTR_INT:   return TaggedType::PtrInt();
        case Kind::PTR_NULL:  return TaggedType::PtrInt();
        case Kind::CONST:     return TaggedType::PtrInt();
        case Kind::TAG_PTR:   return TaggedType::PtrInt();
        case Kind::ADDR:      return TaggedType::PtrInt();
        case Kind::MOD: {
          return that.IsOdd() ? TaggedType::Val() : TaggedType::PtrInt();
        }
      }
      llvm_unreachable("invalid kind");
    }
    case Kind::HEAP: {
      switch (that.k_) {
        case Kind::UNKNOWN:  return TaggedType::Heap();
        case Kind::HEAP:     return TaggedType::Heap();
        case Kind::UNDEF:    return TaggedType::Heap();
        case Kind::ONE:      return TaggedType::Val();
        case Kind::ZERO_ONE: return TaggedType::PtrInt();
        case Kind::INT:      return TaggedType::PtrInt();
        case Kind::YOUNG:    return TaggedType::Heap();
        case Kind::PTR:      return TaggedType::PtrInt();
        case Kind::VAL:      return TaggedType::Val();
        case Kind::PTR_INT:  return TaggedType::PtrInt();
        case Kind::ZERO:     return TaggedType::PtrNull();
        case Kind::PTR_NULL: return TaggedType::PtrNull();
        case Kind::CONST:    return TaggedType::PtrInt();
        case Kind::TAG_PTR:  return TaggedType::PtrInt();
        case Kind::ADDR:     return TaggedType::PtrInt();
        case Kind::MOD: {
          return that.IsOdd() ? TaggedType::Val() : TaggedType::PtrInt();
        }
      }
      llvm_unreachable("invalid kind");
    }
    case Kind::PTR: {
      switch (that.k_) {
        case Kind::HEAP:
        case Kind::PTR:
        case Kind::UNKNOWN:
        case Kind::UNDEF:
        case Kind::YOUNG: {
          return TaggedType::Ptr();
        }
        case Kind::ZERO_ONE:
        case Kind::ONE:
        case Kind::INT: {
          return TaggedType::PtrInt();
        }
        case Kind::VAL:
        case Kind::MOD:
        case Kind::PTR_INT: {
          return TaggedType::PtrInt();
        }
        case Kind::PTR_NULL:
        case Kind::ZERO: {
          return TaggedType::PtrNull();
        }
        case Kind::CONST: llvm_unreachable("not implemented");
        case Kind::TAG_PTR: llvm_unreachable("not implemented");
        case Kind::ADDR: llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid kind");
    }
    case Kind::PTR_INT: return TaggedType::PtrInt();
    case Kind::PTR_NULL: {
      switch (that.k_) {
        case Kind::UNKNOWN:   return TaggedType::PtrNull();
        case Kind::PTR:       return TaggedType::PtrNull();
        case Kind::ZERO:      return TaggedType::PtrNull();
        case Kind::PTR_NULL:  return TaggedType::PtrNull();
        case Kind::ONE:       return TaggedType::PtrInt();
        case Kind::INT:       return TaggedType::PtrInt();
        case Kind::ZERO_ONE:  return TaggedType::PtrInt();
        case Kind::PTR_INT:   return TaggedType::PtrInt();
        case Kind::VAL:       return TaggedType::PtrInt();
        case Kind::HEAP:      llvm_unreachable("not implemented");
        case Kind::UNDEF:     llvm_unreachable("not implemented");
        case Kind::YOUNG:     llvm_unreachable("not implemented");
        case Kind::CONST:     llvm_unreachable("not implemented");
        case Kind::TAG_PTR:   llvm_unreachable("not implemented");
        case Kind::ADDR:      llvm_unreachable("not implemented");
        case Kind::MOD:       llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid kind");
    }
    case Kind::ADDR: {
      llvm_unreachable("not implemented");
    }
    case Kind::TAG_PTR: {
      llvm_unreachable("not implemented");
    }
    case Kind::YOUNG: {
      switch (that.k_) {
        case Kind::UNKNOWN:   return TaggedType::Young();
        case Kind::YOUNG:     return TaggedType::Young();
        case Kind::PTR:       return TaggedType::Ptr();
        case Kind::PTR_INT:   return TaggedType::PtrInt();
        case Kind::ZERO: {
          llvm_unreachable("not implemented");
        }
        case Kind::ZERO_ONE: {
          llvm_unreachable("not implemented");
        }
        case Kind::HEAP:      return TaggedType::Heap();
        case Kind::ONE:       return TaggedType::Val();
        case Kind::VAL:       return TaggedType::Val();
        case Kind::MOD:       llvm_unreachable("not implemented");
        case Kind::INT:       llvm_unreachable("not implemented");
        case Kind::UNDEF:     llvm_unreachable("not implemented");
        case Kind::PTR_NULL:  llvm_unreachable("not implemented");
        case Kind::CONST:     llvm_unreachable("not implemented");
        case Kind::TAG_PTR:   llvm_unreachable("not implemented");
        case Kind::ADDR:      llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid kind");
    }
  }
  llvm_unreachable("invalid kind");
}

// -----------------------------------------------------------------------------
bool TaggedType::operator==(const TaggedType &that) const
{
  return k_ == that.k_;
}

// -----------------------------------------------------------------------------
bool TaggedType::operator<(const TaggedType &that) const
{
  switch (k_) {
    case Kind::UNKNOWN: {
      return true;
    }
    case Kind::ONE: {
      return that.k_ == Kind::ZERO_ONE ||
             that.k_ == Kind::INT ||
             that.k_ == Kind::PTR_INT ||
             that.k_ == Kind::VAL ||
             (that.k_ == Kind::MOD && that.IsOdd());
    }
    case Kind::ZERO: {
      return that.k_ == Kind::INT ||
             that.k_ == Kind::PTR ||
             that.k_ == Kind::VAL ||
             that.k_ == Kind::ZERO_ONE ||
             that.k_ == Kind::PTR_INT ||
             that.k_ == Kind::PTR_NULL ||
             (that.k_ == Kind::MOD && that.IsEven());
    }
    case Kind::ZERO_ONE: {
      return that.k_ == Kind::INT ||
             that.k_ == Kind::PTR_INT;
    }
    case Kind::MOD: {
      return that.k_ == Kind::INT ||
             that.k_ == Kind::PTR_INT ||
             that.k_ == Kind::VAL;
    }
    case Kind::CONST: {
      return that.k_ == Kind::INT ||
             that.k_ == Kind::PTR_INT ||
             that.k_ == Kind::VAL;
    }
    case Kind::INT: {
      return that.k_ == Kind::VAL ||
             that.k_ == Kind::PTR_INT;
    }
    case Kind::VAL: {
      return that.k_ == Kind::PTR ||
             that.k_ == Kind::PTR_INT;
    }
    case Kind::HEAP: {
      return that.k_ == Kind::VAL ||
             that.k_ == Kind::PTR ||
             that.k_ == Kind::PTR_INT;
    }
    case Kind::YOUNG: {
      return that.k_ == Kind::HEAP ||
             that.k_ == Kind::VAL ||
             that.k_ == Kind::PTR ||
             that.k_ == Kind::PTR_INT;
    }
    case Kind::PTR: {
      return that.k_ == Kind::PTR_INT ||
             that.k_ == Kind::PTR_NULL;
    }
    case Kind::PTR_INT: {
      return false;
    }
    case Kind::ADDR: {
      llvm_unreachable("not implemented");
    }
    case Kind::TAG_PTR: {
      llvm_unreachable("not implemented");
    }
    case Kind::UNDEF: {
      return that.k_ != Kind::UNKNOWN;
    }
    case Kind::PTR_NULL: {
      return that.k_ == Kind::PTR_INT;
    }
  }
  llvm_unreachable("invalid kind");
}

// -----------------------------------------------------------------------------
void TaggedType::dump(llvm::raw_ostream &os) const
{
  switch (k_) {
    case Kind::UNKNOWN:      os << "unknown";      return;
    case Kind::ONE:          os << "one";          return;
    case Kind::ZERO:         os << "zero";         return;
    case Kind::ZERO_ONE:     os << "one|zero";     return;
    case Kind::INT:          os << "int";          return;
    case Kind::HEAP:         os << "heap";         return;
    case Kind::YOUNG:        os << "young";        return;
    case Kind::UNDEF:        os << "undef";        return;
    case Kind::VAL:          os << "val";          return;
    case Kind::PTR:          os << "ptr";          return;
    case Kind::PTR_INT:      os << "ptr|int";      return;
    case Kind::PTR_NULL:     os << "ptr|null";     return;
    case Kind::TAG_PTR:      os << "tag_ptr";      return;
    case Kind::ADDR:         os << "addr";         return;
    case Kind::CONST: {
      os << u_.IntVal;
      return;
    }
    case Kind::MOD: {
      os << u_.ModVal.Div << "n+" << u_.ModVal.Rem;
      return;
    }
  }
  llvm_unreachable("invalid kind");
}

// -----------------------------------------------------------------------------
TaggedType TaggedType::Modulo(const Mod &mod)
{
  TaggedType r(Kind::MOD);
  r.u_.ModVal = mod;
  return r;
}
