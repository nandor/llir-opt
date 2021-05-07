// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "passes/tags/tagged_type.h"

using namespace tags;



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
        case Kind::ZERO_OR_ONE: {
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
          llvm_unreachable("not implemented");
        }
        case Kind::YOUNG: {
          llvm_unreachable("not implemented");
        }
        case Kind::UNDEF: {
          llvm_unreachable("not implemented");
        }
        case Kind::INT_OR_PTR:
        case Kind::INT:
        case Kind::ANY: {
          k_ = that.k_;
          return *this;
        }
      }
      llvm_unreachable("invalid kind");
    }
    case Kind::ODD: {
      switch (that.k_) {
        case Kind::UNKNOWN:
        case Kind::ODD:
        case Kind::ONE: {
          return *this;
        }
        case Kind::INT:
        case Kind::EVEN:
        case Kind::ZERO:
        case Kind::ZERO_OR_ONE: {
          k_ = Kind::INT;
          return *this;
        }
        case Kind::HEAP: {
          k_ = Kind::VAL;
          return *this;
        }
        case Kind::PTR: {
          llvm_unreachable("not implemented");
        }
        case Kind::YOUNG: {
          llvm_unreachable("not implemented");
        }
        case Kind::UNDEF: {
          llvm_unreachable("not implemented");
        }
        case Kind::INT_OR_PTR:
        case Kind::VAL:
        case Kind::ANY: {
          k_ = that.k_;
          return *this;
        }
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
        case Kind::ZERO_OR_ONE: {
          k_ = Kind::ZERO_OR_ONE;
          return *this;
        }
        case Kind::VAL:
        case Kind::HEAP: {
          k_ = Kind::VAL;
          return *this;
        }
        case Kind::PTR: {
          llvm_unreachable("not implemented");
        }
        case Kind::YOUNG: {
          llvm_unreachable("not implemented");
        }
        case Kind::UNDEF: {
          llvm_unreachable("not implemented");
        }
        case Kind::INT_OR_PTR:
        case Kind::ODD:
        case Kind::ANY: {
          k_ = that.k_;
          return *this;
        }
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
        case Kind::ZERO_OR_ONE: {
          k_ = Kind::ZERO_OR_ONE;
          return *this;
        }
        case Kind::HEAP: {
          k_ = Kind::VAL;
          return *this;
        }
        case Kind::EVEN:
        case Kind::INT:
        case Kind::INT_OR_PTR:
        case Kind::PTR:
        case Kind::YOUNG:
        case Kind::ANY:
        case Kind::VAL: {
          k_ = that.k_;
          return *this;
        }
      }
      llvm_unreachable("invalid kind");
    }
    case Kind::ZERO_OR_ONE: {
      switch (that.k_) {
        case Kind::ZERO:
        case Kind::ONE:
        case Kind::ZERO_OR_ONE:
        case Kind::UNKNOWN: {
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
          k_ = Kind::INT_OR_PTR;
          return *this;
        }
        case Kind::YOUNG: {
          llvm_unreachable("not implemented");
        }
        case Kind::UNDEF: {
          llvm_unreachable("not implemented");
        }
        case Kind::INT:
        case Kind::INT_OR_PTR:
        case Kind::ANY: {
          k_ = that.k_;
          return *this;
        }
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
        case Kind::ZERO_OR_ONE:
        case Kind::INT: {
          return *this;
        }
        case Kind::VAL: {
          llvm_unreachable("not implemented");
        }
        case Kind::HEAP: {
          llvm_unreachable("not implemented");
        }
        case Kind::PTR: {
          llvm_unreachable("not implemented");
        }
        case Kind::YOUNG: {
          llvm_unreachable("not implemented");
        }
        case Kind::UNDEF: {
          llvm_unreachable("not implemented");
        }
        case Kind::INT_OR_PTR: {
          k_ = Kind::INT_OR_PTR;
          return *this;
        }
        case Kind::ANY: {
          k_ = that.k_;
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
        case Kind::INT: {
          return *this;
        }
        case Kind::ZERO_OR_ONE: {
          llvm_unreachable("not implemented");
        }
        case Kind::EVEN: {
          k_ = Kind::VAL;
          return *this;
        }
        case Kind::PTR:
        case Kind::INT_OR_PTR: {
          k_ = Kind::ANY;
          return *this;
        }
        case Kind::YOUNG: {
          llvm_unreachable("not implemented");
        }
        case Kind::UNDEF:
          llvm_unreachable("not implemented");
        case Kind::ANY: {
          k_ = that.k_;
          return *this;
        }
      }
      llvm_unreachable("invalid kind");
    }
    case Kind::HEAP: {
      switch (that.k_) {
        case Kind::UNKNOWN:
        case Kind::HEAP:
        case Kind::UNDEF: {
          return *this;
        }
        case Kind::ODD:
        case Kind::ONE: {
          k_ = Kind::VAL;
          return *this;
        }
        case Kind::ZERO: {
          llvm_unreachable("not implemented");
        }
        case Kind::ZERO_OR_ONE: {
          llvm_unreachable("not implemented");
        }
        case Kind::EVEN:
          llvm_unreachable("not implemented");
        case Kind::INT:
          llvm_unreachable("not implemented");
        case Kind::YOUNG: {
          llvm_unreachable("not implemented");
        }
        case Kind::PTR: 
        case Kind::VAL:
        case Kind::INT_OR_PTR:
        case Kind::ANY: {
          k_ = that.k_;
          return *this;
        }
      }
      llvm_unreachable("invalid kind");
    }
    case Kind::PTR: {
      switch (that.k_) {
        case Kind::HEAP:
        case Kind::PTR:
        case Kind::ZERO:
        case Kind::UNKNOWN:
        case Kind::UNDEF: {
          return *this;
        }
        case Kind::ZERO_OR_ONE: {
          llvm_unreachable("not implemented");
        }
        case Kind::ODD:
          llvm_unreachable("not implemented");
        case Kind::ONE:
          llvm_unreachable("not implemented");
        case Kind::INT: {
          k_ = Kind::INT_OR_PTR;
          return *this;
        }
        case Kind::VAL: {
          k_ = Kind::INT_OR_PTR;
          return *this;
        }
        case Kind::YOUNG: {
          llvm_unreachable("not implemented");
        }
        case Kind::EVEN: {
          k_ = Kind::INT_OR_PTR;
          return *this;
        }
        case Kind::INT_OR_PTR:
        case Kind::ANY: {
          k_ = that.k_;
          return *this;
        }
      }
      llvm_unreachable("invalid kind");
    }
    case Kind::YOUNG: {
      switch (that.k_) {
        case Kind::UNKNOWN:
        case Kind::YOUNG:
        case Kind::INT_OR_PTR: {
          return *this;
        }
        case Kind::PTR:
          llvm_unreachable("not implemented");
        case Kind::ZERO:
          llvm_unreachable("not implemented");
        case Kind::ZERO_OR_ONE:
          llvm_unreachable("not implemented");
        case Kind::HEAP:
          llvm_unreachable("not implemented");
        case Kind::ODD:
          llvm_unreachable("not implemented");
        case Kind::ONE:
          llvm_unreachable("not implemented");
        case Kind::INT:
          llvm_unreachable("not implemented");
        case Kind::VAL:
          llvm_unreachable("not implemented");
        case Kind::UNDEF:
          llvm_unreachable("not implemented");
        case Kind::EVEN:
          llvm_unreachable("not implemented");
        case Kind::ANY: {
          k_ = that.k_;
          return *this;
        }
      }
      llvm_unreachable("invalid kind");
    }
    case Kind::UNDEF: {
      k_ = that.k_;
      return *this;
    }
    case Kind::INT_OR_PTR: {
      switch (that.k_) {
        case Kind::UNKNOWN:
        case Kind::EVEN:
        case Kind::ODD:
        case Kind::ONE:
        case Kind::INT:
        case Kind::ZERO:
        case Kind::ZERO_OR_ONE:
        case Kind::INT_OR_PTR:
        case Kind::PTR: {
          return *this;
        }
        case Kind::VAL:
        case Kind::HEAP: {
          k_ = Kind::ANY;
          return *this;
        }
        case Kind::UNDEF: {
          return *this;
        }
        case Kind::YOUNG:
        case Kind::ANY: {
          k_ = that.k_;
          return *this;
        }
      }
      llvm_unreachable("invalid kind");
    }
    case Kind::ANY: {
      return *this;
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
             that.k_ == Kind::ZERO_OR_ONE ||
             that.k_ == Kind::INT_OR_PTR ||
             that.k_ == Kind::ANY;
    }
    case Kind::EVEN: {
      return that.k_ == Kind::INT ||
             that.k_ == Kind::INT_OR_PTR ||
             that.k_ == Kind::VAL ||
             that.k_ == Kind::ANY;
    }
    case Kind::ONE: {
      return that.k_ == Kind::ODD ||
             that.k_ == Kind::ZERO_OR_ONE ||
             that.k_ == Kind::INT ||
             that.k_ == Kind::INT_OR_PTR ||
             that.k_ == Kind::VAL ||
             that.k_ == Kind::ANY;
    }
    case Kind::ODD: {
      return that.k_ == Kind::INT ||
             that.k_ == Kind::INT_OR_PTR ||
             that.k_ == Kind::VAL ||
             that.k_ == Kind::ANY;
    }
    case Kind::ZERO_OR_ONE: {
      return that.k_ == Kind::INT ||
             that.k_ == Kind::ANY ||
             that.k_ == Kind::INT_OR_PTR;
    }
    case Kind::INT: {
      return that.k_ == Kind::ANY ||
             that.k_ == Kind::INT_OR_PTR;
    }
    case Kind::VAL: {
      return that.k_ == Kind::PTR ||
             that.k_ == Kind::INT_OR_PTR ||
             that.k_ == Kind::ANY;
    }
    case Kind::HEAP: {
      return that.k_ == Kind::VAL ||
             that.k_ == Kind::PTR ||
             that.k_ == Kind::INT_OR_PTR ||
             that.k_ == Kind::ANY;
    }
    case Kind::YOUNG: {
      return that.k_ == Kind::ANY;
    }
    case Kind::PTR: {
      return that.k_ == Kind::INT_OR_PTR ||
             that.k_ == Kind::ANY;
    }
    case Kind::INT_OR_PTR: {
      return that.k_ == Kind::YOUNG || that.k_ == Kind::ANY;
    }
    case Kind::UNDEF: {
      return that.k_ == Kind::EVEN ||
             that.k_ == Kind::ODD ||
             that.k_ == Kind::INT ||
             that.k_ == Kind::PTR ||
             that.k_ == Kind::VAL ||
             that.k_ == Kind::ZERO ||
             that.k_ == Kind::ONE ||
             that.k_ == Kind::ZERO_OR_ONE ||
             that.k_ == Kind::INT_OR_PTR ||
             that.k_ == Kind::ANY;
    }
    case Kind::ANY: {
      return false;
    }
  }
  llvm_unreachable("invalid kind");
}

// -----------------------------------------------------------------------------
void TaggedType::dump(llvm::raw_ostream &os) const
{
  switch (k_) {
    case Kind::UNKNOWN: {
      os << "unknown";
      return;
    }
    case Kind::EVEN: {
      os << "even";
      return;
    }
    case Kind::ODD: {
      os << "odd";
      return;
    }
    case Kind::ONE: {
      os << "one";
      return;
    }
    case Kind::ZERO: {
      os << "zero";
      return;
    }
    case Kind::ZERO_OR_ONE: {
      os << "one|zero";
      return;
    }
    case Kind::INT: {
      os << "int";
      return;
    }
    case Kind::VAL: {
      os << "val";
      return;
    }
    case Kind::HEAP: {
      os << "heap";
      return;
    }
    case Kind::PTR: {
      os << "ptr";
      return;
    }
    case Kind::YOUNG: {
      os << "young";
      return;
    }
    case Kind::UNDEF: {
      os << "undef";
      return;
    }
    case Kind::INT_OR_PTR: {
      os << "int|ptr";
      return;
    }
    case Kind::ANY: {
      os << "any";
      return;
    }
  }
  llvm_unreachable("invalid kind");
}
