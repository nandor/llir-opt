// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <cassert>
#include <iostream>
#include <sstream>
#include "core/data.h"
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
static inline bool IsDigit(char chr, unsigned base)
{
  switch (base) {
    case 2: {
      return chr == '0' || chr == '1';
    }
    case 8: {
      return '0' <= chr && chr <= '7';
    }
    case 10: {
      return '0' <= chr && chr <= '9';
    }
    case 16: {
      return ('0' <= chr && chr <= '9')
          || ('a' <= chr && chr <= 'f')
          || ('A' <= chr && chr <= 'F');
    }
    default: {
      assert(!"invalid base");
    }
  }
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
Parser::Parser(Context &ctx, const std::string &path)
  : ctx_(ctx)
  , is_(path)
  , char_('\0')
  , tk_(Token::END)
  , row_(1)
  , col_(0)
  , segment_(Segment::DATA)
  , data_(new Data)
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
        Check(Token::NEWLINE);
        continue;
      }
      default: {
        throw ParserError(row_, col_, "unexpected token, expected operation");
      }
    }
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
Value *Parser::ParseValue()
{
  switch (tk_) {
    case Token::MINUS: {
      NextToken();
      Check(Token::NUMBER);
      NextToken();
      return nullptr;
    }
    case Token::NUMBER: {
      NextToken();
      return nullptr;
    }
    case Token::IDENT: {
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
      return nullptr;
    }
    default: {
      throw ParserError(row_, col_, "unexpected token, expected value");
    }
  }
}

// -----------------------------------------------------------------------------
void Parser::ParseDirective()
{
  assert(value_.size() >= 2 && "empty directive");
  std::string op = value_;
  NextToken();

  switch (op[1]) {
    case 'a': {
      if (op == ".addr") return data_->AddSymbol(ParseValue());
      if (op == ".align") return Skip();
      if (op == ".ascii") return Skip();
      if (op == ".asciz") return Skip();
      break;
    }
    case 'b': {
      if (op == ".bss") {
        SwitchSegment(Segment::BSS);
        return;
      }
      if (op == ".byte") return data_->AddInt8(ParseValue());
      break;
    }
    case 'c': {
      if (op == ".comm") return ParseComm();
      break;
    }
    case 'd': {
      if (op == ".data") {
        SwitchSegment(Segment::DATA);
        return;
      }
      if (op == ".double") return data_->AddFloat64(ParseValue());
      break;
    }
    case 'f': {
      if (op == ".file") return Skip();
      break;
    }
    case 'g': {
      if (op == ".globl") return Skip();
      break;
    }
    case 'i': {
      if (op == ".ident") return Skip();
      break;
    }
    case 'l': {
      if (op == ".long") return data_->AddInt32(ParseValue());
      break;
    }
    case 'm': {
      if (op == ".macosx_version_min") return Skip();
      break;
    }
    case 'p': {
      if (op == ".p2align") return Skip();
      break;
    }
    case 'q': {
      if (op == ".quad") return data_->AddInt64(ParseValue());
      break;
    }
    case 's': {
      if (op == ".section") return Skip();
      if (op == ".size") return Skip();
      if (op == ".space") return Skip();
      if (op == ".stack") return Skip();
      break;
    }
    case 't': {
      if (op == ".text") {
        SwitchSegment(Segment::TEXT);
        return;
      }
      if (op == ".type") return Skip();
      break;
    }
    case 'w': {
      if (op == ".weak") return Skip();
      break;
    }
    case 'z': {
      if (op == ".zero") return data_->AddZero(ParseValue());
      break;
    }
  }

  throw ParserError(row_, col_, "unknown directive: " + op);
}

// -----------------------------------------------------------------------------
void Parser::ParseInstruction()
{
  // An instruction is composed of an opcode, followed by optional annotations.
  size_t dot = value_.find('.');
  param_ = dot != std::string::npos ? value_.substr(dot) : "";

  // Decode the opcode.
  Inst::Type op = ParseOpcode(value_.substr(0, dot));

  // Parse all arguments.
  do {
    switch (NextToken()) {
      case Token::NEWLINE: {
        break;
      }
      case Token::REG: {
        NextToken();
        break;
      }
      case Token::LBRACE: {
        Expect(Token::REG);
        Expect(Token::RBRACE);
        NextToken();
        break;
      }
      default: {
        ParseValue();
        break;
      }
    }
  } while (tk_ == Token::COMMA);

  // Parse optional annotations.
  while (tk_ == Token::ANNOT) {
    NextToken();
  }

  // Done, must end with newline.
  Check(Token::NEWLINE);
}

// -----------------------------------------------------------------------------
Inst::Type Parser::ParseOpcode(const std::string &op)
{
  assert(op.size() > 0 && "empty token");
  switch (op[0]) {
    case 'a': {
      if (op == "atomic") return Inst::Type::ATOMIC;
      if (op == "addr") return Inst::Type::ADDR;
      if (op == "arg") return Inst::Type::ARG;
      if (op == "abs") return Inst::Type::ABS;
      if (op == "add") return Inst::Type::ADD;
      if (op == "and") return Inst::Type::AND;
      if (op == "asr") return Inst::Type::ASR;
      break;
    }
    case 'c': {
      if (op == "call") return Inst::Type::CALL;
      if (op == "cmp") return Inst::Type::CMP;
      break;
    }
    case 'd': {
      if (op == "div") return Inst::Type::DIV;
      break;
    }
    case 'i': {
      if (op == "imm") return Inst::Type::IMM;
      break;
    }
    case 'j': {
      if (op == "jf") return Inst::Type::JF;
      if (op == "jt") return Inst::Type::JT;
      if (op == "ji") return Inst::Type::JI;
      if (op == "jmp") return Inst::Type::JMP;
      break;
    }
    case 'l': {
      if (op == "ld") return Inst::Type::LD;
      if (op == "lsl") return Inst::Type::LSL;
      if (op == "lsr") return Inst::Type::LSR;
      break;
    }
    case 'm': {
      if (op == "mov") return Inst::Type::MOV;
      if (op == "mod") return Inst::Type::MOD;
      if (op == "mul") return Inst::Type::MUL;
      if (op == "mulh") return Inst::Type::MULH;
      break;
    }
    case 'n': {
      if (op == "neg") return Inst::Type::NEG;
      break;
    }
    case 'o': {
      if (op == "or") return Inst::Type::OR;
      break;
    }
    case 'p': {
      if (op == "pop") return Inst::Type::POP;
      if (op == "push") return Inst::Type::PUSH;
      break;
    }
    case 'r': {
      if (op == "ret") return Inst::Type::RET;
      if (op == "rem") return Inst::Type::REM;
      if (op == "rotl") return Inst::Type::ROTL;
      break;
    }
    case 's': {
      if (op == "select") return Inst::Type::SELECT;
      if (op == "st") return Inst::Type::ST;
      if (op == "switch") return Inst::Type::SWITCH;
      if (op == "sext") return Inst::Type::SEXT;
      if (op == "shl") return Inst::Type::SHL;
      if (op == "sra") return Inst::Type::SRA;
      if (op == "srl") return Inst::Type::SRL;
      if (op == "sub") return Inst::Type::SUB;
      break;
    }
    case 't': {
      if (op == "tcall") return Inst::Type::TCALL;
      if (op == "trunc") return Inst::Type::TRUNC;
      break;
    }
    case 'x': {
      if (op == "xor") return Inst::Type::XOR;
      break;
    }
    case 'z': {
      if (op == "zext") return Inst::Type::ZEXT;
      break;
    }
  }

  throw ParserError(row_, col_, "unknown opcode: " + op);
}

// -----------------------------------------------------------------------------
void Parser::ParseComm()
{
  // Parse the symbol.
  Check(Token::IDENT);
  Expect(Token::COMMA);

  // Parse the size.
  Expect(Token::NUMBER);
  Expect(Token::COMMA);

  // Parse the alignment.
  Expect(Token::NUMBER);

  // New directive.
  Expect(Token::NEWLINE);
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

// -----------------------------------------------------------------------------
Parser::Token Parser::NextToken()
{
  // Clear the value buffer.
  value_.clear();

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
        unsigned base = 10;
        if (char_ == '0') {
          switch (NextChar()) {
            case 'x': value_ = "0x"; base = 16; NextChar(); break;
            case 'b': value_ = "0b"; base =  2; NextChar(); break;
            case 'o': value_ = "0o"; base =  8; NextChar(); break;
            default: {
              if (!IsDigit(char_)) {
                value_ = "0";
                return tk_ = Token::NUMBER;
              }
              break;
            }
          }
        }
        do {
          value_.push_back(char_);
        } while (IsDigit(NextChar(), base));
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
  NextToken();
  Check(type);
}

// -----------------------------------------------------------------------------
void Parser::Check(Token type)
{
  if (tk_ != type) {
    auto ToString = [](Token tk) {
      switch (tk) {
        case Token::NEWLINE:  return "newline";
        case Token::END:      return "eof";
        case Token::LBRACE:   return "'['";
        case Token::RBRACE:   return "']'";
        case Token::COMMA:    return "','";
        case Token::REG:      return "register";
        case Token::IDENT:    return "identifier";
        case Token::LABEL:    return "label";
        case Token::NUMBER:   return "number";
        case Token::ANNOT:    return "annot";
        case Token::STRING:   return "string";
        case Token::PLUS:     return "'+'";
        case Token::MINUS:    return "'-'";
      }
    };
    throw ParserError(row_, col_)
        << ToString(type) << " expected, got " << ToString(tk_);
  }
}

