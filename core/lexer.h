// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <llvm/ADT/StringRef.h>

#include <string>
#include <fstream>

#include "core/adt/sexp.h"
#include "core/constant.h"

class Prog;
class Func;
class Block;



/**
 * Breaks an assembly source file into tokens.
 */
class Lexer final {
public:
  /// Enumeration of tokens extracted from the stream.
  enum class Token {
    // '\n'
    NEWLINE,
    // End of stream
    END,
    // '['
    LBRACKET,
    // ']'
    RBRACKET,
    // '(',
    LPAREN,
    // ')',
    RPAREN,
    // ','
    COMMA,
    // '$[a-z]+'
    REG,
    // '$[0-9]+'
    VREG,
    // [a-zA-Z_.][a-zA-Z_0-9.]*
    IDENT,
    // [IDENT]:
    COLON,
    // [0-9]+
    NUMBER,
    // @[a-zA-Z0-9_]+
    ANNOT,
    // Quoted string
    STRING,
    // Plus sign.
    PLUS,
    // Minus sign.
    MINUS,
  };

public:
  /// Creates a lexer for a stream.
  Lexer(llvm::StringRef buf);

  /// Cleanup.
  ~Lexer();

  /// Returns the current token.
  Token GetToken() const { return tk_; }
  /// Fetches the next token.
  Token NextToken();
  /// Checks if the next character is of a specific type.
  void Expect(Token type);
  /// Checks if the current token is of a specific type.
  void Check(Token type);
  /// Checks whether the end of stream was reached.
  bool AtEnd() const { return tk_ == Token::END; }

  /// Returns the current string.
  std::string_view String() const;
  /// Returns the current integer.
  int64_t Int() const;
  /// Returns the current register.
  Register Reg() const;
  /// Returns the current virtual register.
  uint64_t VReg() const;

  /// Parses an S-Expression.
  SExp ParseSExp();

  /// Error reporting.
  [[noreturn]] void Error(const std::string &msg);
  [[noreturn]] void Error(Func *f, const std::string &msg);
  [[noreturn]] void Error(Func *f, Block *b, const std::string &msg);

private:
  /// Fetches the next character.
  char NextChar();

private:
  /// Source stream.
  llvm::StringRef buf_;
  /// Pointer to the stream.
  const char *ptr_;
  /// Current character.
  char char_;
  /// Current token.
  Token tk_;
  /// Current row number.
  unsigned row_;
  /// Current column number.
  unsigned col_;
  /// String value stored in the current token.
  std::string str_;
  /// Current register.
  Register reg_;
  /// Current virtual register.
  uint64_t vreg_;
  /// Integer parameter storing the current integer.
  int64_t int_;
};
