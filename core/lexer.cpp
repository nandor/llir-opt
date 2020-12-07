// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <stack>
#include <sstream>

#include "core/lexer.h"
#include "core/func.h"
#include "core/block.h"



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
      llvm_unreachable("invalid base");
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
  llvm_unreachable("invalid digit");
}

// -----------------------------------------------------------------------------
static inline bool IsAlphaNum(char chr)
{
  return IsAlpha(chr) || IsDigit(chr) || chr == '_';
}

// -----------------------------------------------------------------------------
static inline bool IsIdentStart(char chr)
{
  return IsAlpha(chr) || chr == '_' || chr == '.' || chr == '\1';
}

// -----------------------------------------------------------------------------
static inline bool IsIdentCont(char chr)
{
  return IsAlphaNum(chr) || chr == '$' || chr == '@';
}

// -----------------------------------------------------------------------------
Lexer::Lexer(llvm::StringRef buf)
  : buf_(buf)
  , ptr_(buf.data())
  , char_('\0')
  , tk_(Token::END)
  , row_(1)
  , col_(0)
{
  NextChar();
  NextToken();
}

// -----------------------------------------------------------------------------
Lexer::~Lexer()
{
}

// -----------------------------------------------------------------------------
Lexer::Token Lexer::NextToken()
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
    case '[': NextChar(); return tk_ = Token::LBRACKET;
    case ']': NextChar(); return tk_ = Token::RBRACKET;
    case '(': NextChar(); return tk_ = Token::LPAREN;
    case ')': NextChar(); return tk_ = Token::RPAREN;
    case ',': NextChar(); return tk_ = Token::COMMA;
    case '+': NextChar(); return tk_ = Token::PLUS;
    case '-': NextChar(); return tk_ = Token::MINUS;
    case ':': NextChar(); return tk_ = Token::COLON;
    case '$': {
      NextChar();
      if (IsDigit(char_)) {
        vreg_ = 0ull;
        do {
          vreg_ = vreg_ * 10 + ToInt(char_);
        } while (IsDigit(NextChar()));
        return tk_ = Token::VREG;
      } else if (IsAlpha(char_)) {
        do {
          str_.push_back(char_);
        } while (IsAlphaNum(NextChar()));

        static std::vector<std::pair<const char *, ConstantReg::Kind>> regs =
        {
          std::make_pair("sp",           ConstantReg::Kind::SP          ),
          std::make_pair("fs",           ConstantReg::Kind::FS          ),
          std::make_pair("ret_addr",     ConstantReg::Kind::RET_ADDR    ),
          std::make_pair("frame_addr",   ConstantReg::Kind::FRAME_ADDR  ),
          std::make_pair("aarch64_fpsr", ConstantReg::Kind::AARCH64_FPSR),
          std::make_pair("aarch64_fpcr", ConstantReg::Kind::AARCH64_FPCR),
          std::make_pair("riscv_fcsr",   ConstantReg::Kind::RISCV_FCSR  ),
          std::make_pair("riscv_frm",    ConstantReg::Kind::RISCV_FRM   ),
          std::make_pair("riscv_fflags", ConstantReg::Kind::RISCV_FFLAGS),
          std::make_pair("ppc_fpscr",    ConstantReg::Kind::PPC_FPSCR   ),
        };

        for (const auto &reg : regs) {
          if (reg.first == str_) {
            reg_ = reg.second;
            return tk_ = Token::REG;
          }
        }

        Error("unknown register: " + str_);
      } else {
        Error("invalid register name");
      }
    }
    case '@': {
      if (!IsAlphaNum(NextChar())) {
        Error("empty annotation");
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
          switch (NextChar()) {
            case 'b':  str_.push_back('\b'); NextChar(); break;
            case 'f':  str_.push_back('\f'); NextChar(); break;
            case 'n':  str_.push_back('\n'); NextChar(); break;
            case 'r':  str_.push_back('\r'); NextChar(); break;
            case 't':  str_.push_back('\t'); NextChar(); break;
            case '\\': str_.push_back('\\'); NextChar(); break;
            case '\"': str_.push_back('\"'); NextChar(); break;
            default: {
              if (IsDigit(char_, 8)) {
                unsigned chr = 0 ;
                for (int i = 0; i < 3 && IsDigit(char_, 8); ++i, NextChar()) {
                  unsigned nextVal = chr * 8 + char_ - '0';
                  if (nextVal > 256) {
                    break;
                  } else {
                    chr = nextVal;
                  }
                }
                str_.push_back(chr);
              } else {
                Error("invalid escape: " + std::string(1, char_));
              }
              break;
            }
          }
        } else {
          str_.push_back(char_);
          NextChar();
        }
      }
      NextChar();
      return tk_ = Token::STRING;
    }
    default: {
      if (IsIdentStart(char_)) {
        do {
          str_.push_back(char_);
        } while (IsIdentCont(NextChar()) || char_ == '.');
        return tk_ = Token::IDENT;
      } else if (IsDigit(char_)) {
        unsigned base = 10;
        if (char_ == '0') {
          switch (NextChar()) {
            case 'x': base = 16; NextChar(); break;
            case 'b': base =  2; NextChar(); break;
            case 'o': base =  8; NextChar(); break;
            default: {
              if (IsDigit(char_)) {
                Error("invalid numeric constant");
              }
              return tk_ = Token::NUMBER;
            }
          }
        }
        do {
          int_ = int_ * base + ToInt(char_);
        } while (IsDigit(NextChar(), base));
        if (IsAlphaNum(char_)) {
          Error("invalid numeric constant");
        }
        return tk_ = Token::NUMBER;
      } else {
        Error("unexpected char: " + std::string(1, char_));
      }
    }
  }
}

// -----------------------------------------------------------------------------
char Lexer::NextChar()
{
  if (ptr_ == buf_.data() + buf_.size()) {
    char_ = '\0';
    return char_;
  }

  char_ = *ptr_++;
  if (IsNewline(char_)) {
    row_ += 1;
    col_ = 1;
  } else {
    col_ += 1;
  }
  return char_;
}

// -----------------------------------------------------------------------------
void Lexer::Expect(Token type)
{
  NextToken();
  Check(type);
}

// -----------------------------------------------------------------------------
void Lexer::Check(Token type)
{
  if (tk_ != type) {
    auto ToString = [](Token tk) -> std::string {
      switch (tk) {
        case Token::NEWLINE:  return "newline";
        case Token::END:      return "eof";
        case Token::LBRACKET: return "'['";
        case Token::RBRACKET: return "']'";
        case Token::LPAREN:   return "'('";
        case Token::RPAREN:   return "')'";
        case Token::COMMA:    return "','";
        case Token::REG:      return "reg";
        case Token::VREG:     return "vreg";
        case Token::IDENT:    return "identifier";
        case Token::NUMBER:   return "number";
        case Token::ANNOT:    return "annot";
        case Token::STRING:   return "string";
        case Token::PLUS:     return "'+'";
        case Token::MINUS:    return "'-'";
        case Token::COLON:    return "':'";
      }
      llvm_unreachable("invalid token");
    };
    Error(ToString(type) + " expected, got " + ToString(tk_));
  }
}

// -----------------------------------------------------------------------------
std::string_view Lexer::String() const
{
  return str_;
}

// -----------------------------------------------------------------------------
int64_t Lexer::Int() const
{
  return int_;
}

// -----------------------------------------------------------------------------
ConstantReg Lexer::Reg() const
{
  return reg_;
}

// -----------------------------------------------------------------------------
uint64_t Lexer::VReg() const
{
  return vreg_;
}

// -----------------------------------------------------------------------------
SExp Lexer::ParseSExp()
{
  SExp sexp;
  if (tk_ == Token::LPAREN) {
    std::stack<SExp::List *> stk;
    stk.push(sexp.AsList());
    while (!stk.empty()) {
      while (NextToken() != Token::RPAREN) {
        switch (tk_) {
          case Token::NUMBER: {
            stk.top()->AddNumber(int_);
            continue;
          }
          case Token::STRING: {
            stk.top()->AddString(str_);
            continue;
          }
          case Token::LPAREN: {
            stk.push(stk.top()->AddList());
            continue;
          }
          default: {
            Error("invalid token in s-expression");
          }
        }
      }
      stk.pop();
    }
    NextToken();
  }
  return sexp;
}

// -----------------------------------------------------------------------------
[[noreturn]] void Lexer::Error(const std::string &msg)
{
  std::ostringstream os;
  os << "["
     << row_ << ":" << col_
     << "]: " << msg;
  llvm::report_fatal_error(os.str());
}

// -----------------------------------------------------------------------------
[[noreturn]] void Lexer::Error(Func *func, const std::string &msg)
{
  std::ostringstream os;
  os << "["
      << row_ << ":" << col_ << ": " << func->GetName()
     << "]: " << msg;
  llvm::report_fatal_error(os.str());
}

// -----------------------------------------------------------------------------
[[noreturn]] void Lexer::Error(
    Func *func,
    Block *block,
    const std::string &msg)
{
  std::ostringstream os;
  os << "["
     << row_ << ":" << col_ << ": " << func->GetName() << ":"
     << block->GetName()
     << "]: " << msg;
  llvm::report_fatal_error(os.str());
}
