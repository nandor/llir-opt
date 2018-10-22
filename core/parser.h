// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <string>
#include <fstream>
#include "core/inst.h"
#include "core/func.h"
#include "core/prog.h"

class Context;
class Data;



/**
 * Parses an assembly file.
 */
class Parser final {
public:
  Parser(Context &ctx, const std::string &path);
  ~Parser();

  void Parse();

private:
  /// Enumeration of tokens extracted from the stream.
  enum class Token {
    /// '\n'
    NEWLINE,
    /// End of stream
    END,
    /// '['
    LBRACE,
    /// ']'
    RBRACE,
    /// ','
    COMMA,
    /// '$[a-zA-Z0-9_]+'
    REG,
    /// [a-zA-Z_.][a-zA-Z_0-9.]*
    IDENT,
    /// [IDENT]:
    LABEL,
    /// [0-9]+
    NUMBER,
    /// @[a-zA-Z0-9_]+
    ANNOT,
    /// Quoted string
    STRING,
    /// Plus sign.
    PLUS,
    /// Minus sign.
    MINUS,
  };

  /// Parses an identifier.
  void ParseIdent();
  /// Skips to the next line.
  void Skip();
  /// Parses an integer.
  int64_t ParseInteger();
  /// Parses a symbol.
  const char *ParseSymbol();

  /// Parses a directive.
  void ParseDirective();
  /// Parses an instruction.
  void ParseInstruction();

  /// Parses an opcode.
  Inst::Type ParseOpcode(const std::string &op);
  /// Parse a comm directive.
  void ParseComm();

  /// Switches the current segment.
  void SwitchSegment(Segment type);

  /// Allocates an instruction.
  template<typename T>
  void MakeInst();

  /// Fetches the next token.
  Token NextToken();
  /// Fetches the next character.
  char NextChar();
  /// Checks if the next character is of a specific type.
  void Expect(Token type);
  /// Checks if the current token is of a specific type.
  void Check(Token type);

  /// Reference to the parent context.
  Context &ctx_;
  /// Source stream.
  std::ifstream is_;
  /// Current character.
  char char_;
  /// Current token.
  Token tk_;
  /// Current row number.
  unsigned row_;
  /// Current column number.
  unsigned col_;
  /// String value stored in the current token.
  std::string value_;
  /// Parameter part of the token.
  std::string param_;
  /// Current segment.
  Segment segment_;
  /// Current data segment.
  Data *data_;
};
