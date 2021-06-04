// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "passes/tags/tagged_type.h"

using namespace tags;


// -----------------------------------------------------------------------------
TaggedType::TaggedType(const TaggedType &that)
  : k_(that.k_)
{
  switch (k_) {
    case Kind::CONST: {
      new (&u_.IntVal) int64_t(that.u_.IntVal);
      return;
    }
    case Kind::MASK: {
      new (&u_.MaskVal) MaskedType(that.u_.MaskVal);
      return;
    }
    case Kind::INT:
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
      u_.IntVal.~int64_t();
      return;
    }
    case Kind::MASK: {
      u_.MaskVal.~MaskedType();
      return;
    }
    case Kind::INT:
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
      new (&u_.IntVal) int64_t(that.u_.IntVal);
      break;
    }
    case Kind::MASK: {
      new (&u_.MaskVal) MaskedType(that.u_.MaskVal);
      break;
    }
    case Kind::INT:
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
    case Kind::YOUNG:
    case Kind::TAG_PTR: {
      return true;
    }
    case Kind::ZERO_ONE:
    case Kind::INT: {
      return false;
    }
    case Kind::CONST: {
      return (u_.IntVal & 1) == 0;
    }
    case Kind::MASK: {
      return (u_.MaskVal.GetKnown() & 1) == 1 && (u_.MaskVal.GetValue() & 1) == 0;
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
    case Kind::YOUNG:
    case Kind::TAG_PTR:
    case Kind::ZERO_ONE:
    case Kind::INT: {
      return false;
    }
    case Kind::CONST: {
      return (u_.IntVal & 1) == 1;
    }
    case Kind::MASK: {
      return (u_.MaskVal.GetKnown() & 1) == 1 && (u_.MaskVal.GetValue() & 1) == 1;
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
bool TaggedType::IsZero() const
{
  switch (k_) {
    case Kind::CONST: {
      return u_.IntVal == 0;
    }
    case Kind::MASK: {
      auto k = u_.MaskVal.GetKnown();
      auto v = u_.MaskVal.GetValue();
      return k == static_cast<uint64_t>(-1) && v == 0;
    }
    case Kind::YOUNG:
    case Kind::HEAP:
    case Kind::PTR:
    case Kind::ADDR:
    case Kind::TAG_PTR:
    case Kind::ZERO_ONE:
    case Kind::INT:
    case Kind::UNKNOWN:
    case Kind::VAL:
    case Kind::PTR_NULL:
    case Kind::PTR_INT:
    case Kind::UNDEF: {
      return false;
    }
  }
  llvm_unreachable("invalid kind");
}

// -----------------------------------------------------------------------------
bool TaggedType::IsOne() const
{
  switch (k_) {
    case Kind::CONST: {
      return u_.IntVal == 1;
    }
    case Kind::MASK: {
      auto k = u_.MaskVal.GetKnown();
      auto v = u_.MaskVal.GetValue();
      return k == static_cast<uint64_t>(-1) && v == 1;
    }
    case Kind::YOUNG:
    case Kind::HEAP:
    case Kind::PTR:
    case Kind::ADDR:
    case Kind::TAG_PTR:
    case Kind::ZERO_ONE:
    case Kind::INT:
    case Kind::UNKNOWN:
    case Kind::VAL:
    case Kind::PTR_NULL:
    case Kind::PTR_INT:
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
    case Kind::ZERO_ONE:
    case Kind::MASK:
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
    case Kind::MASK: {
      switch (that.k_) {
        case Kind::UNDEF:    return *this;
        case Kind::UNKNOWN:  return *this;
        case Kind::ZERO_ONE: return TaggedType::Int();
        case Kind::INT:      return TaggedType::Int();
        case Kind::PTR:      return TaggedType::PtrInt();
        case Kind::PTR_INT:  return TaggedType::PtrInt();
        case Kind::PTR_NULL: return TaggedType::PtrInt();
        case Kind::TAG_PTR:  return TaggedType::PtrInt();
        case Kind::ADDR:     return TaggedType::PtrInt();
        case Kind::VAL:
        case Kind::HEAP:
        case Kind::YOUNG: {
          return IsOdd() ? TaggedType::Val() : TaggedType::PtrInt();
        }
        case Kind::MASK: {
          auto vl = u_.MaskVal.GetValue();
          auto vr = that.u_.MaskVal.GetValue();
          auto same = ~(vl ^ vr);
          auto k = same & u_.MaskVal.GetKnown() & that.u_.MaskVal.GetKnown();
          return TaggedType::Mask({vl & k, k});
        }
        case Kind::CONST: {
          auto v = u_.MaskVal.GetValue();
          auto k = u_.MaskVal.GetKnown();
          auto i = that.u_.IntVal;
          return TaggedType::Mask({v & i, k & ~(v ^ i)});
        }
      }
      llvm_unreachable("invalid kind");
    }
    case Kind::ZERO_ONE: {
      switch (that.k_) {
        case Kind::UNKNOWN:  return TaggedType::ZeroOne();
        case Kind::UNDEF:    return TaggedType::ZeroOne();
        case Kind::ZERO_ONE: return TaggedType::ZeroOne();
        case Kind::INT:      return TaggedType::Int();
        case Kind::MASK:     return TaggedType::Int();
        case Kind::VAL:      llvm_unreachable("not implemented");
        case Kind::HEAP:     llvm_unreachable("not implemented");
        case Kind::PTR:      return TaggedType::PtrInt();
        case Kind::YOUNG:    return TaggedType::PtrInt();
        case Kind::PTR_INT:  return TaggedType::PtrInt();
        case Kind::PTR_NULL: return TaggedType::PtrInt();
        case Kind::TAG_PTR:  llvm_unreachable("not implemented");
        case Kind::ADDR:     llvm_unreachable("not implemented");
        case Kind::CONST: {
          auto v = that.u_.IntVal;
          if (v & 1) {
            return that;
          } else {
            return TaggedType::Mask({
                static_cast<uint64_t>(v),
                static_cast<uint64_t>(-1) & ~1
            });
          }
        }
      }
      llvm_unreachable("invalid kind");
    }
    case Kind::CONST: {
      auto l = GetConst();
      switch (that.k_) {
        case Kind::UNKNOWN:  return *this;
        case Kind::CONST: {
          auto r = that.GetConst();
          return TaggedType::Mask(MaskedType(l & r, ~(l ^ r)));
        }
        case Kind::MASK: {
          auto v = that.u_.MaskVal.GetValue();
          auto k = that.u_.MaskVal.GetKnown();
          return TaggedType::Mask({v & l, k & ~(v ^ l)});
        }
        case Kind::ZERO_ONE: {
          if (l & 1) {
            return *this;
          } else {
            return TaggedType::Mask({
                static_cast<uint64_t>(l),
                static_cast<uint64_t>(-1) & ~1
            });
          }
        }
        case Kind::INT:      return TaggedType::Int();
        case Kind::UNDEF:    return TaggedType::Int();
        case Kind::VAL:      return TaggedType::PtrInt();
        case Kind::HEAP:     return TaggedType::PtrInt();
        case Kind::YOUNG:    return TaggedType::PtrInt();
        case Kind::PTR:      return TaggedType::PtrInt();
        case Kind::PTR_NULL: llvm_unreachable("not implemented");
        case Kind::PTR_INT:  return TaggedType::PtrInt();
        case Kind::TAG_PTR:  return TaggedType::PtrInt();
        case Kind::ADDR:     return TaggedType::PtrInt();
      }
      llvm_unreachable("invalid kind");
    }
    case Kind::INT: {
      switch (that.k_) {
        case Kind::UNKNOWN:  return TaggedType::Int();
        case Kind::CONST:    return TaggedType::Int();
        case Kind::MASK:      return TaggedType::Int();
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
        case Kind::VAL:       return TaggedType::Val();
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
        case Kind::MASK: {
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
        case Kind::ZERO_ONE: return TaggedType::PtrInt();
        case Kind::INT:      return TaggedType::PtrInt();
        case Kind::YOUNG:    return TaggedType::Heap();
        case Kind::PTR:      return TaggedType::PtrInt();
        case Kind::VAL:      return TaggedType::Val();
        case Kind::PTR_INT:  return TaggedType::PtrInt();
        case Kind::PTR_NULL: return TaggedType::PtrNull();
        case Kind::CONST:    return TaggedType::PtrInt();
        case Kind::TAG_PTR:  return TaggedType::PtrInt();
        case Kind::ADDR:     return TaggedType::PtrInt();
        case Kind::MASK: {
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
        case Kind::INT: {
          return TaggedType::PtrInt();
        }
        case Kind::VAL:
        case Kind::MASK:
        case Kind::PTR_INT:
        case Kind::CONST: {
          return TaggedType::PtrInt();
        }
        case Kind::PTR_NULL: {
          return TaggedType::PtrNull();
        }
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
        case Kind::PTR_NULL:  return TaggedType::PtrNull();
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
        case Kind::MASK:      llvm_unreachable("not implemented");
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
        case Kind::ZERO_ONE:  return TaggedType::PtrInt();
        case Kind::HEAP:      return TaggedType::Heap();
        case Kind::VAL:       return TaggedType::Val();
        case Kind::MASK:      llvm_unreachable("not implemented");
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
  if (k_ != that.k_) {
    return false;
  }
  switch (that.k_) {
    case Kind::MASK: {
      return u_.MaskVal == that.u_.MaskVal;
    }
    case Kind::CONST: {
      return u_.IntVal == that.u_.IntVal;
    }
    case Kind::VAL:
    case Kind::UNKNOWN:
    case Kind::YOUNG:
    case Kind::PTR:
    case Kind::PTR_INT:
    case Kind::ZERO_ONE:
    case Kind::HEAP:
    case Kind::INT:
    case Kind::UNDEF:
    case Kind::PTR_NULL:
    case Kind::TAG_PTR:
    case Kind::ADDR: {
      return true;
    }
  }
  llvm_unreachable("invalid kind");
}

// -----------------------------------------------------------------------------
bool TaggedType::operator<(const TaggedType &that) const
{
  switch (k_) {
    case Kind::UNKNOWN: {
      return true;
    }
    case Kind::ZERO_ONE: {
      return that.k_ == Kind::INT ||
             that.k_ == Kind::PTR_INT;
    }
    case Kind::MASK: {
      auto vl = u_.MaskVal.GetValue();
      auto kl = u_.MaskVal.GetKnown();
			switch (that.k_) {
				case Kind::MASK: {
          if (u_.MaskVal == that.u_.MaskVal) {
            return false;
          } else {
            auto vr = that.u_.MaskVal.GetValue();
            auto kr = that.u_.MaskVal.GetKnown();
            return ((kr & kl) == kr) && ((vr & kr) == (vl & kr));
          }
        }
				case Kind::ZERO_ONE: {
 				 	return (kl & ~1ull) == ~1ull;
				}
			  case Kind::CONST: {
					return false;
        }
				case Kind::PTR_NULL: {
					return kl == static_cast<uint64_t>(-1) && vl == 0;
				}
				case Kind::INT:
				case Kind::PTR_INT: {
					return true;
				}
				case Kind::VAL: {
          return (kl & 1) == 1 && (vl & 1) == 1;
        }
				case Kind::YOUNG:
				case Kind::HEAP:
				case Kind::PTR:
				case Kind::ADDR:
				case Kind::TAG_PTR: {
          // Unordered relative to pointers.
					return false;
        }
				case Kind::UNKNOWN:
				case Kind::UNDEF: {
					// Greater than undef/bot.
          return false;
				}
			}
			llvm_unreachable("invalid type kind");
    }
    case Kind::CONST: {
			switch (that.k_) {
			  case Kind::CONST: {
					return false;
        }
				case Kind::MASK: {
          return (u_.IntVal & that.u_.MaskVal.GetKnown()) == that.u_.MaskVal.GetValue();
        }
				case Kind::INT: {
					return true;
				}
				case Kind::ZERO_ONE: {
 					return u_.IntVal == 0 || u_.IntVal == 1;
				}
				case Kind::PTR_NULL: {
					return u_.IntVal == 0;
				}
				case Kind::PTR_INT: {
					return true;
				}
				case Kind::VAL: {
          return (u_.IntVal & 1) == 1;
        }
				case Kind::YOUNG:
				case Kind::HEAP:
				case Kind::PTR:
				case Kind::ADDR:
				case Kind::TAG_PTR: {
          // Unordered relative to pointers.
					return false;
        }
				case Kind::UNKNOWN:
				case Kind::UNDEF: {
					// Greater than undef/bot.
          return false;
				}
			}
			llvm_unreachable("invalid type kind");
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
    case Kind::CONST:        os << u_.IntVal;      return;
    case Kind::MASK:         os << u_.MaskVal;     return;
  }
}

// -----------------------------------------------------------------------------
TaggedType TaggedType::Mask(const MaskedType &mask)
{
  TaggedType r(Kind::MASK);
  new (&r.u_.MaskVal) MaskedType(mask);
  return r;
}

// -----------------------------------------------------------------------------
TaggedType TaggedType::Const(int64_t v)
{
  TaggedType r(Kind::CONST);
  new (&r.u_.IntVal) int64_t(v);
  return r;
}
