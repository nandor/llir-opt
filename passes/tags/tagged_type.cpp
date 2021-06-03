// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "passes/tags/tagged_type.h"

using namespace tags;



// -----------------------------------------------------------------------------
bool TaggedType::IsIntLike() const
{
  switch (k_) {
    case Kind::ZERO:
    case Kind::EVEN:
    case Kind::ONE:
    case Kind::ODD:
    case Kind::ZERO_ONE:
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
    case Kind::UNDEF: {
      return false;
    }
  }
  llvm_unreachable("invalid kind");
}

// -----------------------------------------------------------------------------
TaggedType &TaggedType::operator|=(const TaggedType &that)
{
  switch (k_) {
    case Kind::UNKNOWN: {
      k_ = that.k_;
      return *this;
    }
    case Kind::EVEN: {
      switch (that.k_) {
        case Kind::UNKNOWN:
        case Kind::EVEN:
        case Kind::ZERO: {
          return *this;
        }
        case Kind::ONE:
        case Kind::ODD:
        case Kind::ZERO_ONE: {
          k_ = Kind::INT;
          return *this;
        }
        case Kind::VAL: {
          llvm_unreachable("not implemented");
        }
        case Kind::HEAP: {
          llvm_unreachable("not implemented");
        }
        case Kind::PTR: {
          k_ = Kind::PTR_INT;
          return *this;
        }
        case Kind::YOUNG: {
          llvm_unreachable("not implemented");
        }
        case Kind::UNDEF: {
          llvm_unreachable("not implemented");
        }
        case Kind::PTR_INT:
        case Kind::INT: {
          k_ = that.k_;
          return *this;
        }

        case Kind::PTR_NULL: llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid kind");
    }
    case Kind::ODD: {
      switch (that.k_) {
        case Kind::UNKNOWN:
        case Kind::ODD:
        case Kind::ONE:
        case Kind::UNDEF: {
          return *this;
        }
        case Kind::INT:
        case Kind::EVEN:
        case Kind::ZERO:
        case Kind::ZERO_ONE: {
          k_ = Kind::INT;
          return *this;
        }
        case Kind::HEAP: 
        case Kind::YOUNG: {
          k_ = Kind::VAL;
          return *this;
        }
        case Kind::PTR: {
          k_ = Kind::PTR_INT;
          return *this;
        }
        case Kind::PTR_INT:
        case Kind::VAL: {
          k_ = that.k_;
          return *this;
        }

        case Kind::PTR_NULL: llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid kind");
    }
    case Kind::ONE: {
      switch (that.k_) {
        case Kind::ONE:
        case Kind::UNKNOWN: {
          return *this;
        }
        case Kind::INT:
        case Kind::EVEN: {
          k_ = Kind::INT;
          return *this;
        }
        case Kind::ZERO:
        case Kind::ZERO_ONE: {
          k_ = Kind::ZERO_ONE;
          return *this;
        }
        case Kind::VAL:
        case Kind::HEAP:
        case Kind::YOUNG: {
          k_ = Kind::VAL;
          return *this;
        }
        case Kind::PTR: {
          k_ = Kind::PTR_INT;
          return *this;
        }
        case Kind::UNDEF: {
          k_ = Kind::ONE;
          return *this;
        }
        case Kind::PTR_INT:
        case Kind::ODD: {
          k_ = that.k_;
          return *this;
        }
        case Kind::PTR_NULL: llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid kind");
    }
    case Kind::ZERO: {
      switch (that.k_) {
        case Kind::ZERO:
        case Kind::UNKNOWN:
        case Kind::UNDEF: {
          return *this;
        }
        case Kind::ODD: {
          k_ = Kind::INT;
          return *this;
        }
        case Kind::ONE:
        case Kind::ZERO_ONE: {
          k_ = Kind::ZERO_ONE;
          return *this;
        }
        case Kind::HEAP: {
          k_ = Kind::VAL;
          return *this;
        }
        case Kind::EVEN:
        case Kind::INT:
        case Kind::PTR_INT:
        case Kind::YOUNG:
        case Kind::VAL: {
          k_ = that.k_;
          return *this;
        }
        case Kind::PTR_NULL:
        case Kind::PTR: {
          k_ = Kind::PTR_NULL;
          return *this;
        }
      }
      llvm_unreachable("invalid kind");
    }
    case Kind::ZERO_ONE: {
      switch (that.k_) {
        case Kind::ZERO:
        case Kind::ONE:
        case Kind::ZERO_ONE:
        case Kind::UNKNOWN:
        case Kind::UNDEF: {
          return *this;
        }
        case Kind::EVEN:
        case Kind::ODD: {
          k_ = Kind::INT;
          return *this;
        }
        case Kind::VAL: {
          llvm_unreachable("not implemented");
        }
        case Kind::HEAP: {
          llvm_unreachable("not implemented");
        }
        case Kind::PTR: {
          k_ = Kind::PTR_INT;
          return *this;
        }
        case Kind::YOUNG: {
          llvm_unreachable("not implemented");
        }
        case Kind::INT:
        case Kind::PTR_INT: {
          k_ = that.k_;
          return *this;
        }

        case Kind::PTR_NULL: llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid kind");
    }
    case Kind::INT: {
      switch (that.k_) {
        case Kind::UNKNOWN:
        case Kind::EVEN:
        case Kind::ZERO:
        case Kind::ODD:
        case Kind::ONE:
        case Kind::ZERO_ONE:
        case Kind::INT:
        case Kind::UNDEF: {
          return *this;
        }
        case Kind::VAL: {
          k_ = Kind::VAL;
          return *this;
        }
        case Kind::HEAP: {
          k_ = Kind::PTR_INT;
          return *this;
        }
        case Kind::YOUNG: {
          llvm_unreachable("not implemented");
        }
        case Kind::PTR:
        case Kind::PTR_NULL:
        case Kind::PTR_INT: {
          k_ = Kind::PTR_INT;
          return *this;
        }
      }
      llvm_unreachable("invalid kind");
    }
    case Kind::VAL: {
      switch (that.k_) {
        case Kind::UNKNOWN:
        case Kind::ODD:
        case Kind::ONE:
        case Kind::VAL:
        case Kind::ZERO:
        case Kind::HEAP:
        case Kind::INT:
        case Kind::UNDEF:
        case Kind::YOUNG: {
          return *this;
        }
        case Kind::ZERO_ONE: {
          llvm_unreachable("not implemented");
        }
        case Kind::EVEN: {
          k_ = Kind::VAL;
          return *this;
        }
        case Kind::PTR:
        case Kind::PTR_INT: {
          k_ = Kind::PTR_INT;
          return *this;
        }
        case Kind::PTR_NULL: {
          llvm_unreachable("not implemented");
        }
      }
      llvm_unreachable("invalid kind");
    }
    case Kind::HEAP: {
      switch (that.k_) {
        case Kind::UNKNOWN:
        case Kind::HEAP:
        case Kind::UNDEF:
        case Kind::ZERO: {
          return *this;
        }
        case Kind::ODD:
        case Kind::ONE: {
          k_ = Kind::VAL;
          return *this;
        }
        case Kind::ZERO_ONE: {
          llvm_unreachable("not implemented");
        }
        case Kind::EVEN:
          llvm_unreachable("not implemented");
        case Kind::INT: {
          k_ = Kind::PTR_INT;
          return *this;
        }
        case Kind::YOUNG: {
          k_ = Kind::HEAP;
          return *this;
        }
        case Kind::PTR:
        case Kind::VAL:
        case Kind::PTR_INT: {
          k_ = that.k_;
          return *this;
        }

        case Kind::PTR_NULL: llvm_unreachable("not implemented");
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
          return *this;
        }
        case Kind::ZERO_ONE:
        case Kind::ODD:
        case Kind::ONE:
        case Kind::INT: {
          k_ = Kind::PTR_INT;
          return *this;
        }
        case Kind::VAL: {
          k_ = Kind::PTR_INT;
          return *this;
        }
        case Kind::EVEN: {
          k_ = Kind::PTR_INT;
          return *this;
        }
        case Kind::PTR_INT: {
          k_ = that.k_;
          return *this;
        }
        case Kind::PTR_NULL:
        case Kind::ZERO: {
          k_ = Kind::PTR_NULL;
          return *this;
        }
      }
      llvm_unreachable("invalid kind");
    }
    case Kind::YOUNG: {
      switch (that.k_) {
        case Kind::UNKNOWN:
        case Kind::YOUNG: {
          return *this;
        }
        case Kind::PTR: {
          k_ = Kind::PTR;
          return *this;
        }
        case Kind::PTR_INT: {
          k_ = Kind::PTR_INT;
          return *this;
        }
        case Kind::ZERO:
          llvm_unreachable("not implemented");
        case Kind::ZERO_ONE:
          llvm_unreachable("not implemented");
        case Kind::HEAP: {
          k_ = Kind::HEAP;
          return *this;
        }
        case Kind::ODD:
        case Kind::ONE:
        case Kind::VAL: {
          k_ = Kind::VAL;
          return *this;
        }
        case Kind::INT:
          llvm_unreachable("not implemented");
        case Kind::UNDEF:
          llvm_unreachable("not implemented");
        case Kind::EVEN:
          llvm_unreachable("not implemented");
        case Kind::PTR_NULL:
          llvm_unreachable("not implemented");
      }
      llvm_unreachable("invalid kind");
    }
    case Kind::UNDEF: {
      switch (that.k_) {
        case Kind::UNKNOWN: {
          return *this;
        }
        case Kind::EVEN:
        case Kind::ODD:
        case Kind::ONE:
        case Kind::INT:
        case Kind::ZERO:
        case Kind::ZERO_ONE:
        case Kind::PTR_INT:
        case Kind::PTR:
        case Kind::PTR_NULL:
        case Kind::VAL:
        case Kind::HEAP:
        case Kind::UNDEF:
        case Kind::YOUNG: {
          k_ = that.k_;
          return *this;
        }
      }
      llvm_unreachable("invalid kind");
    }
    case Kind::PTR_INT: {
      return *this;
    }
    case Kind::PTR_NULL:{
      switch (that.k_) {
        case Kind::PTR:
        case Kind::ZERO:
        case Kind::PTR_NULL:
        case Kind::UNKNOWN: {
          return *this;
        }
        case Kind::EVEN:
        case Kind::ODD:
        case Kind::ONE:
        case Kind::INT:
        case Kind::ZERO_ONE: {
          k_ = Kind::PTR_INT;
          return *this;
        }
        case Kind::PTR_INT: {
          k_ = that.k_;
          return *this;
        }
        case Kind::VAL: {
          k_ = Kind::PTR_INT;
          return *this;
        }
        case Kind::HEAP: llvm_unreachable("not implemented");
        case Kind::UNDEF: llvm_unreachable("not implemented");
        case Kind::YOUNG: llvm_unreachable("not implemented");
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
    case Kind::ZERO: {
      return that.k_ == Kind::EVEN ||
             that.k_ == Kind::INT ||
             that.k_ == Kind::PTR ||
             that.k_ == Kind::VAL ||
             that.k_ == Kind::ZERO_ONE ||
             that.k_ == Kind::PTR_INT ||
             that.k_ == Kind::PTR_NULL;
    }
    case Kind::EVEN: {
      return that.k_ == Kind::INT ||
             that.k_ == Kind::PTR_INT ||
             that.k_ == Kind::VAL;
    }
    case Kind::ONE: {
      return that.k_ == Kind::ODD ||
             that.k_ == Kind::ZERO_ONE ||
             that.k_ == Kind::INT ||
             that.k_ == Kind::PTR_INT ||
             that.k_ == Kind::VAL;
    }
    case Kind::ODD: {
      return that.k_ == Kind::INT ||
             that.k_ == Kind::PTR_INT ||
             that.k_ == Kind::VAL;
    }
    case Kind::ZERO_ONE: {
      return that.k_ == Kind::INT ||
             that.k_ == Kind::PTR_INT;
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
    case Kind::UNDEF: {
      return that.k_ == Kind::EVEN ||
             that.k_ == Kind::ODD ||
             that.k_ == Kind::INT ||
             that.k_ == Kind::PTR ||
             that.k_ == Kind::VAL ||
             that.k_ == Kind::ZERO ||
             that.k_ == Kind::ONE ||
             that.k_ == Kind::ZERO_ONE ||
             that.k_ == Kind::PTR_INT;
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
    case Kind::UNKNOWN:  os << "unknown";   return;
    case Kind::EVEN:     os << "even";      return;
    case Kind::ODD:      os << "odd";       return;
    case Kind::ONE:      os << "one";       return;
    case Kind::ZERO:     os << "zero";      return;
    case Kind::ZERO_ONE: os << "one|zero";  return;
    case Kind::INT:      os << "int";       return;
    case Kind::HEAP:     os << "heap";      return;
    case Kind::YOUNG:    os << "young";     return;
    case Kind::UNDEF:    os << "undef";     return;
    case Kind::VAL:      os << "val";       return;
    case Kind::PTR:      os << "ptr";       return;
    case Kind::PTR_INT:  os << "ptr|int";   return;
    case Kind::PTR_NULL: os << "ptr|null";  return;
  }
  llvm_unreachable("invalid kind");
}
