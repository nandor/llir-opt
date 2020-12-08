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

#include "core/adt/hash.h"
#include "core/adt/sexp.h"
#include "core/calling_conv.h"
#include "core/inst.h"
#include "core/lexer.h"
#include "core/visibility.h"
#include "core/xtor.h"

class Atom;
class Block;
class Context;
class Data;
class Const;
class Func;
class Prog;
class Object;



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
  /// Parses a directive.
  void ParseDirective(const std::string_view op);
  // Segment directives.
  void ParseSection();
  // Other directives.
  void ParseAlign();
  void ParseP2Align();
  void ParseEnd();
  void ParseSpace();
  void ParseAscii();
  void ParseAsciz();
  void ParseQuad();
  void ParseComm(Visibility visibility);
  void ParseInt8();
  void ParseInt16();
  void ParseInt32();
  void ParseDouble();
  // Function and segment attributes.
  void ParseFeatures();
  void ParseStackObject();
  void ParseCall();
  void ParseArgs();
  void ParseVararg();
  void ParseVisibility();
  void ParseNoInline();
  void ParseGlobl();
  void ParseHidden();
  void ParseWeak();
  // Ignored directives.
  void ParseFile();
  void ParseLocal();
  void ParseIdent();
  void ParseAddrsig();
  void ParseAddrsigSym();
  void ParseProtected();
  /// Weak aliases.
  void ParseSet();
  void ParseExtern();
  void ParseEqu();
  /// Constructors/Destructors.
  void ParseXtor(Xtor::Kind kind);

  /// Ensures we are in a data segment.
  void InData();

  /// Create a new block.
  void CreateBlock(const std::string_view name);
  /// Parses an instruction.
  void ParseInstruction(const std::string_view op);
  /// Factory method for instructions.
  Inst *CreateInst(
      const std::string &op,
      const std::vector<Ref<Value>> &ops,
      const std::vector<std::pair<unsigned, TypeFlag>> &flags,
      const std::optional<Cond> &ccs,
      const std::optional<size_t> &sizes,
      const std::vector<Type> &ts,
      const std::optional<CallingConv> &conv,
      bool strict,
      AnnotSet &&annot
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

  /// Parses a positive or negative number.
  int64_t Number();

  /// Parses a string to a token.
  template<typename T>
  T ParseToken(
      const std::vector<std::pair<const char *, T>> &options,
      const std::string_view str
  );

  /// Parses a type.
  Type ParseType(std::string_view str);
  /// Parses a flag type declaration.
  TypeFlag ParseTypeFlags(const std::string_view op);
  /// Parses a calling convention name.
  CallingConv ParseCallingConv(const std::string_view str);
  /// Parses a visibility setting name.
  Visibility ParseVisibility(const std::string_view str);
  /// Strips GOT/PLT attributes from a name.
  std::string_view ParseName(std::string_view ident);

private:
  /// Alias to the token.
  using Token = Lexer::Token;
  /// Underlying lexer.
  Lexer l_;

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
  /// Current mapping of vregs to instructions.
  std::unordered_map<ConstRef<Inst>, unsigned> vregs_;

  /// Set of global symbols.
  std::unordered_set<std::string> globls_;
  /// Set of hidden symbols.
  std::unordered_set<std::string> hidden_;
  /// Set of weak symbols.
  std::unordered_set<std::string> weak_;

  /// Next available ID number.
  uint64_t nextLabel_;

  /// Block names to fix up.
  std::vector<std::pair<std::string, Use **>> fixups_;
};
