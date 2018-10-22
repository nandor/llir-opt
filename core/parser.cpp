// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <cassert>
#include <iostream>
#include <sstream>
#include "core/insts.h"
#include "core/parser.h"



class ParserError final : public std::exception {
public:
  /// Constructs a new error object.
  ParserError(unsigned row, unsigned col)
    : ParserError(row, col, "")
  {
  }

  /// Constructs a new error object.
  ParserError(unsigned row, unsigned col, const std::string &message)
  {
    std::ostringstream os;
    os << "[" << row << "," << col << "]: " << message;
    message_ = os.str();
  }

  /// Appends a string to the message.
  ParserError &operator << (const std::string &str)
  {
    message_ += str;
    return *this;
  }

  /// Returns the error message.
  const char *what() const throw ()
  {
    return message_.c_str();
  }

private:
  /// Error message.
  std::string message_;
};



// -----------------------------------------------------------------------------
static inline bool IsSpace(char chr)
{
  return chr == ' ' || chr == '\t' || chr == '\v';
}

// -----------------------------------------------------------------------------
static inline bool IsNewline(char chr)
{
  return chr == '\n';
}

// -----------------------------------------------------------------------------
static inline bool IsAlpha(char chr)
{
  return ('a' <= chr && chr <= 'z')
      || ('A' <= chr && chr <= 'Z')
      || chr == '_';
}

// -----------------------------------------------------------------------------
static inline bool IsDigit(char chr)
{
  return '0' <= chr && chr <= '9';
}

// -----------------------------------------------------------------------------
static inline bool IsAlphaNum(char chr)
{
  return IsAlpha(chr) || IsDigit(chr) || chr == '_';
}

// -----------------------------------------------------------------------------
static inline bool IsIdentStart(char chr)
{
  return IsAlpha(chr) || chr == '_' || chr == '.';
}

// -----------------------------------------------------------------------------
static inline bool IsIdentCont(char chr)
{
  return IsAlphaNum(chr) || chr == '$' || chr == '@';
}

// -----------------------------------------------------------------------------
Parser::Parser(const std::string &path)
  : is_(path)
  , char_('\0')
  , tk_(Token::END)
  , row_(1)
  , col_(0)
  , segment_(Segment::DATA)
{
  NextChar();
  NextToken();
}

// -----------------------------------------------------------------------------
Parser::~Parser()
{
}

// -----------------------------------------------------------------------------
void Parser::Parse()
{
  while (tk_ != Token::END) {
    switch (tk_) {
      case Token::NEWLINE: {
        NextToken();
        continue;
      }
      case Token::LABEL: {
        if (segment_ == Segment::TEXT) {
          if (!value_.empty() && value_[0] == '.') {
            // Start a new basic block.
          } else {
            // Start a new function.
          }
        } else {
          // Pointer into the data segment.
        }
        NextToken();
        continue;
      }
      case Token::IDENT: {
        if (!value_.empty() && value_[0] == '.') {
          ParseDirective();
        } else {
          ParseInstruction();
        }
        continue;
      }
      default: {
        throw ParserError(row_, col_, "unexpected token");
      }
    }
  }
}

// -----------------------------------------------------------------------------
Parser::Token Parser::NextToken()
{
  // Skip whitespaces and newlines, coalesce multiple newlines into one.
  bool isNewline = false;
  while (IsSpace(char_) || IsNewline(char_) || char_ == '#') {
    while (IsSpace(char_)) NextChar();
    if (char_ == '#') {
      while (NextChar() != '\n');
    }
    if (char_ == '\n') {
      isNewline = true;
      NextChar();
      continue;
    }
  }
  if (isNewline) {
    return tk_ = Token::NEWLINE;
  }

  // Clear the value buffer.
  value_.clear();

  // Anything but newline.
  switch (char_) {
    case '\0': return tk_ = Token::END;
    case '[': NextChar(); return tk_ = Token::LBRACE;
    case ']': NextChar(); return tk_ = Token::RBRACE;
    case ',': NextChar(); return tk_ = Token::COMMA;
    case '+': NextChar(); return tk_ = Token::PLUS;
    case '-': NextChar(); return tk_ = Token::MINUS;
    case '$': {
      NextChar();
      if (!IsAlphaNum(char_)) {
        throw ParserError(row_, col_, "expected a register");
      }
      ParseIdent();
      return tk_ = Token::REG;
    }
    case '@': {
      NextChar();
      if (!IsAlphaNum(NextChar())) {
        throw ParserError(row_, col_, "empty annotation");
      }
      ParseIdent();
      return tk_ = Token::ANNOT;
    }
    case '\"': {
      NextChar();
      while (char_ != '\"') {
        if (char_ == '\\') {
          value_.push_back(char_);
          value_.push_back(NextChar());
        } else {
          value_.push_back(char_);
        }
        NextChar();
      }
      NextChar();
      return tk_ = Token::STRING;
    }
    default: {
      if (IsIdentStart(char_)) {
        do {
          value_.push_back(char_);
        } while (IsIdentCont(NextChar()) || char_ == '.');

        if (char_ == ':') {
          NextChar();
          return tk_ = Token::LABEL;
        } else {
          return tk_ = Token::IDENT;
        }
      } else if (IsDigit(char_)) {
        do {
          value_.push_back(char_);
        } while (IsDigit(NextChar()));
        return tk_ = Token::NUMBER;
      } else {
        throw ParserError(row_, col_, "unexpected character");
      }
    }
  }

  return Token::END;
}

// -----------------------------------------------------------------------------
char Parser::NextChar()
{
  char ch = is_.get();
  if (IsNewline(ch)) {
    row_ += 1;
    col_ = 1;
  } else {
    col_ += 1;
  }
  char_ = ch == EOF ? '\0' : ch;
  return char_;
}

// -----------------------------------------------------------------------------
void Parser::Expect(Token type)
{
  if (NextToken() != type) {
    std::string token;
    switch (type) {
      case Token::NEWLINE:  token = "newline";    break;
      case Token::END:      token = "eof";        break;
      case Token::LBRACE:   token = "'{'";        break;
      case Token::RBRACE:   token = "'}'";        break;
      case Token::COMMA:    token = "','";        break;
      case Token::REG:      token = "register";   break;
      case Token::IDENT:    token = "identifier"; break;
      case Token::LABEL:    token = "label";      break;
      case Token::NUMBER:   token = "number";     break;
      case Token::ANNOT:    token = "annot";      break;
      case Token::STRING:   token = "string";     break;
      case Token::PLUS:     token = "'+'";        break;
      case Token::MINUS:    token = "'-'";        break;
    }
    throw ParserError(row_, col_) << token << " expected";
  }
}

// -----------------------------------------------------------------------------
void Parser::Newline()
{
  if (NextToken() != Token::NEWLINE) {
    throw ParserError(row_, col_, "newline expected");
  }
}

// -----------------------------------------------------------------------------
void Parser::ParseIdent()
{
  do {
    value_.push_back(char_);
  } while (IsAlphaNum(NextChar()) || char_ == '.');
}

// -----------------------------------------------------------------------------
void Parser::Skip() {
  while (tk_ != Token::NEWLINE) {
    if (NextToken() == Token::END) {
      return;
    }
  }
}

// -----------------------------------------------------------------------------
void Parser::ParseDirective()
{
  assert(value_.size() >= 2 && "empty directive");
  switch (value_[1]) {
    case 'a': {
      if (value_ == ".addr") return Skip();
      if (value_ == ".align") return Skip();
      if (value_ == ".ascii") return Skip();
      if (value_ == ".asciz") return Skip();
      break;
    }
    case 'b': {
      if (value_ == ".bss") {
        SwitchSegment(Segment::BSS);
        Newline();
        return;
      }
      if (value_ == ".byte") return Skip();
      break;
    }
    case 'c': {
      if (value_ == ".comm") return ParseComm();
      break;
    }
    case 'd': {
      if (value_ == ".data") {
        SwitchSegment(Segment::DATA);
        Newline();
        return;
      }
      break;
    }
    case 'f': {
      if (value_ == ".file") return Skip();
      if (value_ == ".float64") return Skip();
      break;
    }
    case 'g': {
      if (value_ == ".globl") return Skip();
      break;
    }
    case 'i': {
      if (value_ == ".ident") return Skip();
      if (value_ == ".int32") return Skip();
      if (value_ == ".int64") return Skip();
      if (value_ == ".int8") return Skip();
      break;
    }
    case 'l': {
      if (value_ == ".long") return Skip();
      break;
    }
    case 'm': {
      if (value_ == ".macosx_version_min") return Skip();
      break;
    }
    case 'p': {
      if (value_ == ".p2align") return Skip();
      break;
    }
    case 'q': {
      if (value_ == ".quad") return Skip();
      break;
    }
    case 's': {
      if (value_ == ".section") return Skip();
      if (value_ == ".size") return Skip();
      if (value_ == ".space") return Skip();
      if (value_ == ".stack") return Skip();
      break;
    }
    case 't': {
      if (value_ == ".text") {
        SwitchSegment(Segment::TEXT);
        Newline();
        return;
      }
      if (value_ == ".type") return Skip();
      break;
    }
    case 'w': {
      if (value_ == ".weak") return Skip();
      break;
    }
    case 'z': {
      if (value_ == ".zero") return Skip();
      break;
    }
  }

  throw ParserError(row_, col_, "unknown directive: " + value_);
}

// -----------------------------------------------------------------------------
void Parser::ParseInstruction()
{
  size_t dot = value_.find('.');
  param_ = dot != std::string::npos ? value_.substr(dot) : "";
  value_ = value_.substr(0, dot);

  assert(value_.size() > 0 && "empty token");
  switch (value_[0]) {
    case 'a': {
      if (value_ == "atomic") {
        Skip();
        return;
      }
      if (value_ == "addr") return ParseAddr();
      if (value_ == "arg") return Skip();
      if (value_ == "abs") return ParseUnary(Inst::Type::ABS);
      if (value_ == "add") return ParseBinary(Inst::Type::ADD);
      if (value_ == "and") return ParseBinary(Inst::Type::AND);
      if (value_ == "asr") return ParseBinary(Inst::Type::ASR);
      break;
    }
    case 'c': {
      if (value_ == "call") {
        Skip();
        return;
      }
      if (value_ == "cmp") {
        Skip();
        return;
      }
      break;
    }
    case 'd': {
      if (value_ == "div") return ParseBinary(Inst::Type::DIV);
      break;
    }
    case 'i': {
      if (value_ == "imm") return Skip();
      break;
    }
    case 'j': {
      if (value_ == "jf") {
        Skip();
        return;
      }
      if (value_ == "jt") {
        Skip();
        return;
      }
      if (value_ == "ji") {
        Skip();
        return;
      }
      if (value_ == "jmp") {
        Skip();
        return;
      }
      break;
    }
    case 'l': {
      if (value_ == "ld") {
        Skip();
        return;
      }
      if (value_ == "lsl") return ParseBinary(Inst::Type::LSL);
      if (value_ == "lsr") return ParseBinary(Inst::Type::LSR);
      break;
    }
    case 'm': {
      if (value_ == "mov") return ParseUnary(Inst::Type::MOV);
      if (value_ == "mod") return ParseBinary(Inst::Type::MOD);
      if (value_ == "mul") return ParseBinary(Inst::Type::MUL);
      if (value_ == "mulh") return ParseBinary(Inst::Type::MULH);
      break;
    }
    case 'n': {
      if (value_ == "neg") return ParseUnary(Inst::Type::NEG);
      break;
    }
    case 'o': {
      if (value_ == "or") return ParseBinary(Inst::Type::OR);
      break;
    }
    case 'p': {
      if (value_ == "pop") {
        Skip();
        return;
      }
      if (value_ == "push") {
        Skip();
        return;
      }
      break;
    }
    case 'r': {
      if (value_ == "ret") {
        Skip();
        return;
      }
      if (value_ == "rem") return ParseBinary(Inst::Type::REM);
      if (value_ == "rotl") return ParseBinary(Inst::Type::ROTL);
      break;
    }
    case 's': {
      if (value_ == "select") {
        Skip();
        return;
      }
      if (value_ == "st") {
        Skip();
        return;
      }
      if (value_ == "switch") {
        Skip();
        return;
      }
      if (value_ == "sext") return ParseUnary(Inst::Type::SEXT);
      if (value_ == "shl") return ParseBinary(Inst::Type::SHL);
      if (value_ == "sra") return ParseBinary(Inst::Type::SRA);
      if (value_ == "srl") return ParseBinary(Inst::Type::SRL);
      if (value_ == "sub") return ParseBinary(Inst::Type::SUB);
      break;
    }
    case 't': {
      if (value_ == "tcall") {
        Skip();
        return;
      }
      if (value_ == "trunc") return ParseUnary(Inst::Type::TRUNC);
      break;
    }
    case 'x': {
      if (value_ == "xor") return ParseBinary(Inst::Type::XOR);
      break;
    }
    case 'z': {
      if (value_ == "zext") return ParseUnary(Inst::Type::ZEXT);
      break;
    }
  }

  throw ParserError(row_, col_, "unknown instruction: " + value_);
}

// -----------------------------------------------------------------------------
void Parser::ParseBinary(Inst::Type type)
{
  // Parse the destination.
  Expect(Token::REG);
  Expect(Token::COMMA);

  // Parse the LHS.
  Expect(Token::REG);
  Expect(Token::COMMA);

  // Parse the RHS.
  Expect(Token::REG);

  // Parse annotations.
  while (NextToken() == Token::ANNOT) {
  }

  switch (type) {
    case Inst::Type::ADD:  return MakeInst<AddInst>();
    case Inst::Type::AND:  return MakeInst<AndInst>();
    case Inst::Type::ASR:  return MakeInst<AsrInst>();
    case Inst::Type::DIV:  return MakeInst<DivInst>();
    case Inst::Type::LSL:  return MakeInst<LslInst>();
    case Inst::Type::LSR:  return MakeInst<LsrInst>();
    case Inst::Type::MOD:  return MakeInst<ModInst>();
    case Inst::Type::MUL:  return MakeInst<MulInst>();
    case Inst::Type::MULH: return MakeInst<MulhInst>();
    case Inst::Type::OR:   return MakeInst<OrInst>();
    case Inst::Type::REM:  return MakeInst<RemInst>();
    case Inst::Type::ROTL: return MakeInst<RotlInst>();
    case Inst::Type::SHL:  return MakeInst<ShlInst>();
    case Inst::Type::SRA:  return MakeInst<SraInst>();
    case Inst::Type::SRL:  return MakeInst<SrlInst>();
    case Inst::Type::SUB:  return MakeInst<SubInst>();
    case Inst::Type::XOR:  return MakeInst<XorInst>();
    default: assert(!"unreachable");
  }
}

// -----------------------------------------------------------------------------
void Parser::ParseUnary(Inst::Type type)
{
  // Parse the destination.
  Expect(Token::REG);
  Expect(Token::COMMA);

  // Parse the source.
  Expect(Token::REG);

  // Parse annotations.
  while (NextToken() == Token::ANNOT) {
  }

  switch (type) {
    case Inst::Type::ABS:   return MakeInst<AbsInst>();
    case Inst::Type::MOV:   return MakeInst<MovInst>();
    case Inst::Type::NEG:   return MakeInst<NegInst>();
    case Inst::Type::SEXT:  return MakeInst<SignExtendInst>();
    case Inst::Type::TRUNC: return MakeInst<TruncateInst>();
    case Inst::Type::ZEXT:  return MakeInst<ZeroExtendInst>();
    default: assert(!"unreachable");
  }
}

// -----------------------------------------------------------------------------
void Parser::ParseAddr()
{
  // Parse the destination.
  Expect(Token::REG);
  Expect(Token::COMMA);

  // Parse the symbol.
  Expect(Token::IDENT);

  switch (NextToken()) {
    case Token::PLUS: {
      Expect(Token::NUMBER);
      NextToken();
      break;
    }
    case Token::MINUS: {
      Expect(Token::NUMBER);
      NextToken();
      break;
    }
    default: {
      break;
    }
  }

  // Parse annotations.
  while (tk_ == Token::ANNOT) {
    NextToken();
  }

  return MakeInst<AddrInst>();
}

// -----------------------------------------------------------------------------
void Parser::ParseComm()
{
  // Parse the symbol.
  Expect(Token::IDENT);
  Expect(Token::COMMA);

  // Parse the size.
  Expect(Token::NUMBER);
  Expect(Token::COMMA);

  // Parse the alignment.
  Expect(Token::NUMBER);

  // New directive.
  Newline();
}

// -----------------------------------------------------------------------------
void Parser::SwitchSegment(Segment type)
{
  segment_ = type;
}

// -----------------------------------------------------------------------------
template<typename T> void Parser::MakeInst()
{
  if (segment_ != Segment::TEXT) {
    throw ParserError(row_, col_, "instruction not in text segment");
  }
}
