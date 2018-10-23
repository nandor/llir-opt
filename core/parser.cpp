// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <cassert>
#include <iostream>
#include <sstream>
#include "core/block.h"
#include "core/data.h"
#include "core/func.h"
#include "core/insts.h"
#include "core/parser.h"
#include "core/prog.h"



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

  /// Appends a string to the message.
  ParserError &operator << (char chr)
  {
    std::ostringstream os;
    os << message_ << "\'" << chr << "\'";
    message_ = os.str();
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
static inline bool IsDigit(char chr, unsigned base = 10)
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
static inline int ToInt(char chr)
{
  if ('0' <= chr && chr <= '9') {
    return chr - '0';
  }
  if ('a' <= chr && chr <= 'f') {
    return chr - 'a' + 10;
  }
  if ('A' <= chr && chr <= 'F') {
    return chr - 'A' + 10;
  }
  assert(!"invalid digit");
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
  , prog_(new Prog)
  , data_(nullptr)
  , func_(nullptr)
  , block_(nullptr)
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
        if (data_ == nullptr) {
          if (!str_.empty() && str_[0] == '.') {
            // Start a new basic block.
            InFunc();
            block_ = func_->AddBlock(str_);
          } else {
            // Start a new function.
            func_ = prog_->AddFunc(str_);
            block_ = func_->AddBlock();
          }
        } else {
          // Pointer into the data segment.
        }
        Expect(Token::NEWLINE);
        continue;
      }
      case Token::IDENT: {
        if (!str_.empty() && str_[0] == '.') {
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
  assert(str_.size() >= 2 && "empty directive");
  std::string op = str_;
  NextToken();

  switch (op[1]) {
    case 'a': {
      if (op == ".align") return ParseAlign();
      if (op == ".ascii") return ParseAscii();
      if (op == ".asciz") return ParseAsciz();
      break;
    }
    case 'b': {
      if (op == ".bss") return ParseBSS();
      if (op == ".byte") { InData(); return data_->AddInt8(ParseValue()); }
      break;
    }
    case 'c': {
      if (op == ".comm") return ParseComm();
      if (op == ".const") return ParseConst();
      break;
    }
    case 'd': {
      if (op == ".data") return ParseData();
      if (op == ".double") { InData(); return data_->AddFloat64(ParseValue()); }
      break;
    }
    case 'g': {
      if (op == ".globl") return ParseGlobl();
      break;
    }
    case 'l': {
      if (op == ".long") { InData(); return data_->AddInt32(ParseValue()); }
      break;
    }
    case 'p': {
      if (op == ".p2align") return ParseAlign();
      break;
    }
    case 'q': {
      if (op == ".quad") { InData(); return data_->AddInt64(ParseValue()); }
      break;
    }
    case 's': {
      if (op == ".space") return ParseSpace();
      if (op == ".stack") return ParseStack();
      break;
    }
    case 't': {
      if (op == ".text") return ParseText();
      break;
    }
    case 'w': {
      if (op == ".weak") return ParseWeak();
      break;
    }
    case 'z': {
      if (op == ".zero") { InData(); return data_->AddZero(ParseValue()); }
      break;
    }
  }

  throw ParserError(row_, col_, "unknown directive: ") << op;
}

// -----------------------------------------------------------------------------
void Parser::ParseInstruction()
{
  // Make sure instruction is in text.
  InFunc();

  // An instruction is composed of an opcode, followed by optional annotations.
  size_t dot = str_.find('.');
  param_ = dot != std::string::npos ? str_.substr(dot) : "";

  // Decode the opcode.
  Inst::Type op = ParseOpcode(str_.substr(0, dot));

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

  // Add the instruction to the block.
  block_->AddInst(op);
}

// -----------------------------------------------------------------------------
void Parser::ParseBSS()
{
  data_ = prog_->GetBSS();
  func_ = nullptr;
  block_ = nullptr;
}

// -----------------------------------------------------------------------------
void Parser::ParseData()
{
  data_ = prog_->GetData();
  func_ = nullptr;
  block_ = nullptr;
}

// -----------------------------------------------------------------------------
void Parser::ParseConst()
{
  data_ = prog_->GetConst();
  func_ = nullptr;
  block_ = nullptr;
}

// -----------------------------------------------------------------------------
void Parser::ParseText()
{
  data_ = nullptr;
  func_ = nullptr;
  block_ = nullptr;
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

  throw ParserError(row_, col_, "unknown opcode: ") << op;
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
void Parser::ParseAlign()
{
  InData();
  Check(Token::NUMBER);
  data_->Align(int_);
  Expect(Token::NEWLINE);
}

// -----------------------------------------------------------------------------
void Parser::ParseGlobl()
{
  Check(Token::IDENT);
  Expect(Token::NEWLINE);
}

// -----------------------------------------------------------------------------
void Parser::ParseSpace()
{
  Check(Token::NUMBER);
  Expect(Token::NEWLINE);
}

// -----------------------------------------------------------------------------
void Parser::ParseStack()
{
  Check(Token::NUMBER);
  Expect(Token::NEWLINE);
}

// -----------------------------------------------------------------------------
void Parser::ParseWeak()
{
  Check(Token::IDENT);
  Expect(Token::NEWLINE);
}

// -----------------------------------------------------------------------------
void Parser::ParseAscii()
{
  Check(Token::STRING);
  Expect(Token::NEWLINE);
}

// -----------------------------------------------------------------------------
void Parser::ParseAsciz()
{
  Check(Token::STRING);
  Expect(Token::NEWLINE);
}

// -----------------------------------------------------------------------------
void Parser::InData()
{
  if (data_ == nullptr || func_ != nullptr) {
    throw ParserError(row_, col_, "not in a data segment");
  }
}

// -----------------------------------------------------------------------------
void Parser::InFunc()
{
  if (data_ != nullptr || func_ == nullptr || block_ == nullptr) {
    throw ParserError(row_, col_, "not in a text segment");
  }
}

// -----------------------------------------------------------------------------
Parser::Token Parser::NextToken()
{
  // Clear the value buffer.
  str_.clear();
  int_ = 0;

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
      do {
        str_.push_back(char_);
      } while (IsAlphaNum(NextChar()) || char_ == '.');
      return tk_ = Token::REG;
    }
    case '@': {
      NextChar();
      if (!IsAlphaNum(NextChar())) {
        throw ParserError(row_, col_, "empty annotation");
      }
      do {
        str_.push_back(char_);
      } while (IsAlphaNum(NextChar()) || char_ == '.');
      return tk_ = Token::ANNOT;
    }
    case '\"': {
      NextChar();
      while (char_ != '\"') {
        if (char_ == '\\') {
          str_.push_back(char_);
          str_.push_back(NextChar());
        } else {
          str_.push_back(char_);
        }
        NextChar();
      }
      NextChar();
      return tk_ = Token::STRING;
    }
    default: {
      if (IsIdentStart(char_)) {
        do {
          str_.push_back(char_);
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
            case 'x': base = 16; NextChar(); break;
            case 'b': base =  2; NextChar(); break;
            case 'o': base =  8; NextChar(); break;
            default: {
              if (IsDigit(char_)) {
                throw ParserError(row_, col_, "invalid numeric constant");
              }
              return tk_ = Token::NUMBER;
            }
          }
        }
        do {
          int_ = int_ * base + ToInt(char_);
        } while (IsDigit(NextChar(), base));
        if (IsAlphaNum(char_)) {
          throw ParserError(row_, col_, "invalid numeric constant");
        }
        return tk_ = Token::NUMBER;
      } else {
        throw ParserError(row_, col_, "unexpected character: ") << char_;
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
