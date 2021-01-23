// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/cast.h"
#include "passes/pre_eval/symbolic_context.h"
#include "passes/pre_eval/symbolic_eval.h"
#include "passes/pre_eval/symbolic_value.h"
#include "passes/pre_eval/symbolic_visitor.h"



// -----------------------------------------------------------------------------
bool SymbolicEval::VisitTruncInst(TruncInst &i)
{
  auto arg = ctx_.Find(i.GetArg());
  switch (arg.GetKind()) {
    case SymbolicValue::Kind::UNDEFINED: {
      return SetUndefined();
    }
    case SymbolicValue::Kind::SCALAR:
    case SymbolicValue::Kind::LOWER_BOUNDED_INTEGER: {
      return SetScalar();
    }
    case SymbolicValue::Kind::MASKED_INTEGER: {
      llvm_unreachable("not implemented");
    }
    case SymbolicValue::Kind::INTEGER: {
      return SetInteger(arg.GetInteger().trunc(GetBitWidth(i.GetType())));
    }
    case SymbolicValue::Kind::FLOAT: {
      llvm_unreachable("not implemented");
    }
    case SymbolicValue::Kind::POINTER: {
      uint64_t align = 1 << 16;
      std::optional<int64_t> offset;
      for (const auto &addr : *arg.GetPointer()) {
        switch (addr.GetKind()) {
          case SymbolicAddress::Kind::OBJECT: {
            auto &a = addr.AsObject();
            auto &obj = ctx_.GetObject(a.Object);
            align = std::min(align, obj.GetAlignment().value());
            if (offset) {
              llvm_unreachable("not implemented");
            } else {
              offset = a.Offset & (align - 1);
            }
            continue;
          }
          case SymbolicAddress::Kind::EXTERN: {
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
          case SymbolicAddress::Kind::OBJECT_RANGE:
          case SymbolicAddress::Kind::EXTERN_RANGE: {
            align = 1;
            offset = 0;
            break;
          }
        }
        break;
      }

      if (align > 1 && offset) {
        unsigned bits = GetBitWidth(i.GetType());
        APInt known(bits, align - 1, true);
        APInt value(bits, *offset);
        return SetMask(known, value);
      } else {
        return SetScalar();
      }
    }
    case SymbolicValue::Kind::VALUE:
    case SymbolicValue::Kind::NULLABLE: {
      return SetScalar();
    }
  }
  llvm_unreachable("invalid value kind");
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitZExtInst(ZExtInst &i)
{
  auto arg = ctx_.Find(i.GetArg());
  switch (arg.GetKind()) {
    case SymbolicValue::Kind::SCALAR: {
      return SetScalar();
    }
    case SymbolicValue::Kind::UNDEFINED: {
      return SetUndefined();
    }
    case SymbolicValue::Kind::LOWER_BOUNDED_INTEGER: {
      return SetLowerBounded(arg.GetInteger());
    }
    case SymbolicValue::Kind::MASKED_INTEGER: {
      llvm_unreachable("not implemented");
    }
    case SymbolicValue::Kind::INTEGER: {
      switch (i.GetType()) {
        case Type::I8:
        case Type::I16:
        case Type::I32:
        case Type::I64:
        case Type::V64:
        case Type::I128: {
          return SetInteger(arg.GetInteger().zext(GetBitWidth(i.GetType())));
        }
        case Type::F64: {
          return SetScalar();
        }
        case Type::F32:
        case Type::F80:
        case Type::F128: {
          llvm_unreachable("not implemented");
        }
      }
      llvm_unreachable("invalid type");
    }
    case SymbolicValue::Kind::POINTER:
    case SymbolicValue::Kind::VALUE:
    case SymbolicValue::Kind::NULLABLE: {
      return SetValue(arg.GetPointer()->Decay());
    }
    case SymbolicValue::Kind::FLOAT: {
      llvm_unreachable("not implemented");
    }
  }
  llvm_unreachable("invalid value kind");
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitSExtInst(SExtInst &i)
{
  auto arg = ctx_.Find(i.GetArg());
  switch (arg.GetKind()) {
    case SymbolicValue::Kind::SCALAR: {
      return SetScalar();
    }
    case SymbolicValue::Kind::UNDEFINED: {
      return SetUndefined();
    }
    case SymbolicValue::Kind::LOWER_BOUNDED_INTEGER: {
      return SetLowerBounded(arg.GetInteger());
    }
    case SymbolicValue::Kind::MASKED_INTEGER: {
      llvm_unreachable("not implemented");
    }
    case SymbolicValue::Kind::INTEGER: {
      return SetInteger(arg.GetInteger().sext(GetBitWidth(i.GetType())));
    }
    case SymbolicValue::Kind::POINTER:
    case SymbolicValue::Kind::VALUE:
    case SymbolicValue::Kind::NULLABLE: {
      return SetValue(arg.GetPointer()->Decay());
    }
    case SymbolicValue::Kind::FLOAT: {
      llvm_unreachable("not implemented");
    }
  }
  llvm_unreachable("invalid value kind");
}

// -----------------------------------------------------------------------------
bool SymbolicEval::VisitBitCastInst(BitCastInst &i)
{
  auto v = ctx_.Find(i.GetArg());
  switch (i.GetType()) {
    case Type::I8:
    case Type::I16:
    case Type::I32:
    case Type::I64:
    case Type::V64:
    case Type::I128: {
      llvm_unreachable("not implemented");
    }
    case Type::F64: {
      switch (v.GetKind()) {
        case SymbolicValue::Kind::SCALAR:
        case SymbolicValue::Kind::LOWER_BOUNDED_INTEGER:
        case SymbolicValue::Kind::MASKED_INTEGER: {
          llvm_unreachable("not implemented");
        }
        case SymbolicValue::Kind::INTEGER: {
          return SetFloat(APFloat(
              APFloat::IEEEdouble(),
              v.GetInteger()
          ));
        }
        case SymbolicValue::Kind::VALUE:
        case SymbolicValue::Kind::POINTER:
        case SymbolicValue::Kind::NULLABLE: {
          llvm_unreachable("not implemented");
        }
        case SymbolicValue::Kind::UNDEFINED: {
          llvm_unreachable("not implemented");
        }
        case SymbolicValue::Kind::FLOAT: {
          llvm_unreachable("not implemented");
        }
      }
      llvm_unreachable("not implemented");
    }
    case Type::F32:
    case Type::F80:
    case Type::F128: {
      llvm_unreachable("not implemented");
    }
  }
  llvm_unreachable("invalid type");
}
