// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <list>
#include <string>
#include <fstream>
#include <optional>
#include <unordered_map>

#include "core/attr.h"
#include "core/inst.h"

class Block;
class Context;
class Data;
class Const;
class Func;
class Prog;



/**
 * Parses an assembly file.
 */
class Parser final {
public:
  /**
   * Initialises the parser.
   * @param path Path to the source file.
   */
  Parser(const std::string &path);

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
    // '\n'
    NEWLINE,
    // End of stream
    END,
    // '['
    LBRACE,
    // ']'
    RBRACE,
    // ','
    COMMA,
    // '$[a-z]+'
    REG,
    // '$[0-9]+'
    VREG,
    // [a-zA-Z_.][a-zA-Z_0-9.]*
    IDENT,
    // [IDENT]:
    LABEL,
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

  /// Parses a directive.
  void ParseDirective();
  // Segment directives.
  void ParseData();
  void ParseCode();
  // Other directives.
  void ParseAlign();
  void ParseExtern();
  void ParseEnd();
  void ParseSpace();
  void ParseAscii();
  void ParseQuad();
  // Function and segment attributes.
  void ParseStack();
  void ParseStackObject();
  void ParseCall();
  void ParseArgs();
  void ParseVisibility();
  void ParseNoInline();

  /// Ensures we are in a data segment.
  void InData();
  /// Ensures we are in a text segment.
  void InFunc();

  /// Parses an instruction.
  void ParseInstruction();
  /// Factory method for instructions.
  Inst *CreateInst(
      const std::string &op,
      const std::vector<Value *> &ops,
      const std::optional<Cond> &ccs,
      const std::optional<size_t> &sizes,
      const std::vector<Type> &ts,
      const std::optional<CallingConv> &conv,
      AnnotSet annot
  );
  /// Creates a new block.
  Block *CreateBlock(Func *func, const std::string_view name);
  /// Returns the current function.
  Func *GetFunction();
  /// Returns the current basic block.
  Block *GetBlock();
  /// Ends parsing a function, fixing up vregs.
  void EndFunction();

  /// Parses a calling convention name.
  CallingConv ParseCallingConv(const std::string_view str);
  /// Parses a visibility setting name.
  Visibility ParseVisibility(const std::string_view str);

  /// Fetches the next token.
  Token NextToken();
  /// Fetches the next character.
  char NextChar();
  /// Checks if the next character is of a specific type.
  void Expect(Token type);
  /// Checks if the current token is of a specific type.
  void Check(Token type);

  /// Parses a string to a token.
  template<typename T>
  T ParseToken(
      const std::vector<std::pair<const char *, T>> &options,
      const std::string_view str
  );

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
  ConstantReg::Kind reg_;
  /// Current virtual register.
  uint64_t vreg_;
  /// Integer parameter storing the current integer.
  int64_t int_;
  /// Parameter part of the token.
  std::string param_;
  /// Alignment for some functions.
  std::optional<unsigned> align_;

  /// Current program.
  Prog *prog_;
  /// Current data segment.
  Data *data_;
  /// Current function.
  Func *func_;
  /// Current basic block.
  Block *block_;
  /// Current function name.
  std::optional<std::string> funcName_;
  /// Current mapping of vregs to instructions.
  std::unordered_map<Inst *, unsigned> vregs_;
  /// Current mapping of labels to basic blocks.
  std::unordered_map<std::string, Block *> blocks_;
  /// Basic blocks in their original order.
  std::vector<Block *> topo_;

  /// Next available ID number.
  uint64_t nextLabel_;

  /// Mapping from names to blocks.
  std::unordered_map<std::string_view, Block *> labels_;
  /// Block names to fix up.
  std::vector<Symbol *> fixups_;
};
