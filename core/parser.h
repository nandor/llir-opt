// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <string>
#include <fstream>
#include "core/inst.h"
#include "core/func.h"
#include "core/prog.h"



/**
 * Parses an assembly file.
 */
class Parser final {
public:
  Parser(const std::string &path);
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

  /// Fetches the next token.
  Token NextToken();
  /// Fetches the next character.
  char NextChar();
  /// Checks if the next token is a newline.
  void Newline();
  /// Checks if the next character is of a specific type.
  void Expect(Token type);
  /// Parses an identifier.
  void ParseIdent();
  /// Skips to the next line.
  void Skip();

  /// Parses a directive.
  void ParseDirective();
  /// Parses an instruction.
  void ParseInstruction();

  /// Parse a binary instruction.
  void ParseBinary(Inst::Type type);
  /// Parse a unary instruction.
  void ParseUnary(Inst::Type type);
  /// Parse an addr instruction.
  void ParseAddr();

  /// Parse a comm directive.
  void ParseComm();

  /// Switches the current segment.
  void SwitchSegment(Segment type);

  /// Allocates an instruction.
  template<typename T>
  void MakeInst();

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
};
