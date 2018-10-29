// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <string>
#include <fstream>
#include <unordered_map>
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
  /**
   * Initialises the parser.
   * @param ctx  Optimisation context.
   * @param path Path to the source file.
   */
  Parser(Context &ctx, const std::string &path);

  /**
   * Frees resources used by the parser.
   */
  ~Parser();

  /**
   * Parses the input file.
   */
  Prog *Parse();

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
    /// '$[a-z]+'
    REG,
    /// '$[0-9]+'
    VREG,
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

  /// Parses an instruction.
  void ParseInstruction();
  /// Parses an opcode.
  Inst::Kind ParseOpcode(const std::string_view op);
  /// Returns an instruction mapped to a vreg or creates a dummy.
  Inst *GetVReg(uint64_t vreg);
  /// Maps an instruction to a vreg.
  void SetVReg(uint64_t vreg, Inst *i);
  /// Factory method for instructions.
  Inst *CreateInst(
      Inst::Kind type,
      const std::vector<Operand> &ops,
      const std::optional<Cond> &ccs,
      const std::optional<size_t> &sizes,
      const std::vector<Type> &ts
  );

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
  /// Current register.
  Reg reg_;
  /// Current virtual register.
  uint64_t vreg_;
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
  /// Current mapping of vregs to instructions.
  std::unordered_map<uint64_t, Inst *> vregs_;
  /// Current mapping of labels to basic blocks.
  std::unordered_map<std::string, Block *> blocks_;
};
