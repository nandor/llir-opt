// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <string>
#include <fstream>
#include "core/inst.h"

class Block;
class Context;
class Data;
class Value;
class Func;
class Prog;



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

  /// Parses a constant value.
  Value *ParseValue();

  /// Parses a directive.
  void ParseDirective();
  /// Parses an instruction.
  void ParseInstruction();

  /// Segment directives.
  void ParseBSS();
  void ParseData();
  void ParseConst();
  void ParseText();
  // Other directives.
  void ParseComm();
  void ParseAlign();
  void ParseGlobl();
  void ParseSpace();
  void ParseStack();
  void ParseWeak();
  void ParseAscii();
  void ParseAsciz();

  /// Ensures we are in a data segment.
  void InData();
  /// Ensures we are in a text segment.
  void InFunc();

  /// Parses an opcode.
  Inst::Type ParseOpcode(const std::string &op);

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
  std::string str_;
  /// Integer parameter storing the current integer.
  int64_t int_;
  /// Parameter part of the token.
  std::string param_;
  /// Current program.
  Prog *prog_;
  /// Current data segment.
  Data *data_;
  /// Current function.
  Func *func_;
  /// Current basic block.
  Block *block_;
};
