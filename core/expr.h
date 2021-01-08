// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <cstdint>

#include "core/user.h"

class Context;
class Global;



/**
 * Expression operand.
 */
class Expr : public User {
public:
  /// Kind of the global.
  static constexpr Value::Kind kValueKind = Value::Kind::EXPR;

public:
  /// Enumeration of expression kinds.
  enum Kind {
    /// Fixed offset starting at a symbol.
    SYMBOL_OFFSET,
  };

  /// Destroys the expression.
  virtual ~Expr();

  /// Returns the expression kind.
  Kind GetKind() const { return kind_; }

  /// Checks if the expression is of a given kind.
  bool Is(Kind kind) const { return GetKind() == kind; }

protected:
  /// Constructs a new expression.
  Expr(Kind kind, unsigned numOps)
    : User(Value::Kind::EXPR, numOps)
    , kind_(kind)
  {
  }

private:
  /// Expression kind.
  const Kind kind_;
};


/**
 * Symbol offset expression.
 */
class SymbolOffsetExpr final : public Expr {
public:
  /// Kind of the expression.
  static constexpr Expr::Kind kExprKind = Expr::Kind::SYMBOL_OFFSET;

public:
  /// Creates a new symbol offset expression.
  static SymbolOffsetExpr *Create(Global *sym, int64_t offset);
  /// Cleanup.
  ~SymbolOffsetExpr();

  /// Returns the symbol.
  const Global *GetSymbol() const;
  /// Returns the symbol.
  Global *GetSymbol();

  /// Returns the offset.
  int64_t GetOffset() const { return offset_; }

private:
  /// Allocates a new symbol offset expression.
  SymbolOffsetExpr(Global *sym, int64_t offset);

private:
  /// Offset into the symbol.
  int64_t offset_;
};
