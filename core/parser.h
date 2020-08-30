// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <llvm/ADT/StringRef.h>

#include <list>
#include <string>
#include <fstream>
#include <optional>
#include <unordered_set>
#include <unordered_map>

#include "core/calling_conv.h"
#include "core/visibility.h"
#include "core/inst.h"

class Atom;
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
   * @param buf   Buffer to read from.
   * @param ident Name to identify the module.
   */
  Parser(llvm::StringRef buf, std::string_view ident);

  /**
   * Frees resources used by the parser.
   */
  ~Parser();

  /**
   * Parses the input file.
   */
  std::unique_ptr<Prog> Parse();

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
  void ParseText();
  void ParseBss();
  void ParseSection();
  // Other directives.
  void ParseAlign();
  void ParseP2Align();
  void ParseExtern();
  void ParseEnd();
  void ParseSpace();
  void ParseAscii();
  void ParseAsciz();
  void ParseQuad();
  void ParseComm();
  void ParseInt8();
  void ParseInt16();
  void ParseInt32();
  void ParseDouble();
  // Function and segment attributes.
  void ParseStack();
  void ParseStackObject();
  void ParseCall();
  void ParseArgs();
  void ParseVararg();
  void ParseVisibility();
  void ParseNoInline();
  void ParseNoDeadStrip();
  void ParseGlobl();
  void ParseHidden();
  void ParseWeak();
  // Ignored directives.
  void ParseFile();
  void ParseLocal();
  void ParseIdent();
  void ParseAddrsig();
  void ParseAddrsigSym();
  /// Weak aliases.
  void ParseSet();

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
  /// Returns the current object.
  Object *GetObject();
  /// Returns the current atom.
  Atom *GetAtom();
  /// Returns the current function.
  Func *GetFunction();
  /// Returns the current basic block.
  Block *GetBlock();
  /// Ends parsing a function, fixing up vregs.
  void EndFunction();

  /// Places PHI nodes in a function.
  void PhiPlacement();

  /// Parses a calling convention name.
  CallingConv ParseCallingConv(const std::string_view str);
  /// Parses a visibility setting name.
  Visibility ParseVisibility(const std::string_view str);
  /// Parses a positive or negative number.
  int64_t Number();

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

private:
  [[noreturn]] void ParserError(const std::string &msg);
  [[noreturn]] void ParserError(Func *f, const std::string &msg);
  [[noreturn]] void ParserError(Func *f, Block *b, const std::string &msg);

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
  ConstantReg::Kind reg_;
  /// Current virtual register.
  uint64_t vreg_;
  /// Integer parameter storing the current integer.
  int64_t int_;
  /// Parameter part of the token.
  std::string param_;
  /// Alignment for some functions.
  std::optional<unsigned> funcAlign_;
  /// Alignment for data items.
  std::optional<unsigned> dataAlign_;

  /// Current program.
  std::unique_ptr<Prog> prog_;
  /// Current data segment.
  Data *data_;
  /// Current object.
  Object *object_;
  /// Current atom.
  Atom *atom_;
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

  /// Set of global symbols.
  std::unordered_set<std::string> globls_;
  /// Set of hidden symbols.
  std::unordered_set<std::string> hidden_;
  /// Set of weak symbols.
  std::unordered_set<std::string> weak_;
  /// Set of exported symbols.
  std::unordered_set<std::string> export_;

  /// Next available ID number.
  uint64_t nextLabel_;

  /// Block names to fix up.
  std::vector<std::pair<std::string, Use **>> fixups_;
};
