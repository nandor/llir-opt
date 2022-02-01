// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#pragma once

#include <llvm/ADT/StringRef.h>

#include <fstream>
#include <list>
#include <optional>
#include <stack>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "core/adt/hash.h"
#include "core/adt/sexp.h"
#include "core/calling_conv.h"
#include "core/error.h"
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
private:
  /// Alias to the token.
  using Token = Lexer::Token;
  /// Mapping from instructions to their corresponding vregs.
  using VRegMap = std::unordered_map<ConstRef<Inst>, unsigned>;

  struct Section {
    /// Alignment for some functions.
    std::optional<unsigned> Align;
    /// Current data segment.
    Data *D = nullptr;
    /// Current object.
    Object *O = nullptr;
    /// Current atom.
    Atom *A = nullptr;
    /// Current function.
    Func *F = nullptr;
    /// Current mapping of vregs to instructions.
    VRegMap VRegs;

    Section(Data *data) : D(data) {}
  };

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
  void ParseSection(bool push);
  // Other directives.
  void ParseAlign();
  void ParseP2Align();
  void ParseEnd();
  void ParseSpace();
  void ParseAscii();
  void ParseAsciz();
  void ParseItem(Type ty);
  void ParseItems(Type ty);
  void ParseComm(Visibility visibility);
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
  void ParseThreadLocal();
  void ParsePopSection();
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
  /// Constructors/Destructors.
  void ParseXtor(Xtor::Kind kind);

  /// Create a new block.
  void CreateBlock(Func *func, const std::string_view name);

  /// Returns the current section.
  Section &GetSection();
  /// Returns the current object.
  Object *GetOrCreateObject();
  /// Returns the current atom.
  Atom *GetOrCreateAtom();
  /// Returns the current function.
  Func *GetFunction();
  /// Ends parsing a function, fixing up vregs.
  void EndFunction();
  /// Ends the current function or atom.
  void Align(int64_t alignment);
  /// Ends everything.
  void End();

  /// Places PHI nodes in a function.
  [[nodiscard]] static llvm::Error PhiPlacement(Func &func, VRegMap vregs);

  /// Parses a string to a token.
  template<typename T>
  T ParseToken(
      const std::vector<std::pair<const char *, T>> &options,
      const std::string_view str
  );

  /// Parses a positive or negative number.
  int64_t Number();
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
  /// Representation of an operand parsed from text.
  class Operand {
  public:
    enum class Kind {
      REGISTER,
      VALUE,
    };

  public:

    Operand(Register reg) : kind_(Kind::REGISTER), reg_(reg) {}
    Operand(Value *val) : kind_(Kind::VALUE), val_(val) {}

    std::optional<uintptr_t> ToVReg() const
    {
      if (kind_ == Kind::VALUE) {
        auto val = reinterpret_cast<uintptr_t>(val_);
        return (val & 1) == 1 ? std::optional<uintptr_t>(val) : std::nullopt;
      }
      return {};
    }

    std::optional<Value *> ToVal() const
    {
      if (kind_ == Kind::VALUE) return val_;
      return {};
    }

    std::optional<Register> ToReg() const
    {
      if (kind_ == Kind::REGISTER) return reg_;
      return {};
    }

  private:
    Kind kind_;

    union {
      Value *val_;
      Register reg_;
    };
  };

  /// Parses an instruction.
  void ParseInstruction(
      const std::string_view op,
      Func *func,
      VRegMap &vregs
  );
  /// Factory method for instructions.
  Inst *CreateInst(
      Func *func,
      Block *block,
      const std::string &op,
      const std::vector<Operand> &ops,
      const std::vector<std::pair<unsigned, TypeFlag>> &flags,
      const std::optional<Cond> &ccs,
      const std::optional<size_t> &sizes,
      const std::vector<Type> &ts,
      const std::optional<CallingConv> &conv,
      bool strict,
      AnnotSet &&annot
  );
  /// Parses an annotation.
  void ParseAnnotation(const std::string_view name, AnnotSet &annot);

private:
  /// Underlying lexer.
  Lexer l_;
  /// Current program.
  std::unique_ptr<Prog> prog_;
  /// Next available ID number.
  uint64_t nextLabel_;
  /// Set of global symbols.
  std::unordered_set<std::string> globls_;
  /// Set of hidden symbols.
  std::unordered_set<std::string> hidden_;
  /// Set of weak symbols.
  std::unordered_set<std::string> weak_;
  /// Stack of sections.
  std::stack<Section> stk_;
};
