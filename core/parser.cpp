// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <cassert>
#include <array>
#include <optional>
#include <queue>
#include <sstream>
#include <stack>
#include <string_view>
#include <vector>

#include <llvm/ADT/PostOrderIterator.h>
#include <llvm/ADT/SmallPtrSet.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/constant.h"
#include "core/data.h"
#include "core/func.h"
#include "core/insts.h"
#include "core/parser.h"
#include "core/prog.h"
#include "core/analysis/dominator.h"



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
static std::string_view ParseName(std::string_view ident)
{
  return ident.substr(0, ident.find('@'));
}

// -----------------------------------------------------------------------------
static std::vector<std::pair<const char *, Visibility>> kVisibility
{
  std::make_pair("hidden", Visibility::HIDDEN),
  std::make_pair("extern", Visibility::EXTERN),
  std::make_pair("weak", Visibility::WEAK),
};

// -----------------------------------------------------------------------------
static std::vector<std::pair<const char *, CallingConv>> kCallingConv
{
  std::make_pair("c",          CallingConv::C),
  std::make_pair("caml",       CallingConv::CAML),
  std::make_pair("caml_alloc", CallingConv::CAML_ALLOC),
  std::make_pair("caml_gc",    CallingConv::CAML_GC),
  std::make_pair("caml_raise", CallingConv::CAML_RAISE),
  std::make_pair("setjmp",     CallingConv::SETJMP),
};

// -----------------------------------------------------------------------------
Parser::Parser(llvm::StringRef buf, std::string_view ident)
  : buf_(buf)
  , ptr_(buf.data())
  , char_('\0')
  , tk_(Token::END)
  , row_(1)
  , col_(0)
  , prog_(new Prog(ident))
  , data_(nullptr)
  , atom_(nullptr)
  , func_(nullptr)
  , block_(nullptr)
  , nextLabel_(0)
{
  NextChar();
  NextToken();
}

// -----------------------------------------------------------------------------
Parser::~Parser()
{
}

// -----------------------------------------------------------------------------
std::unique_ptr<Prog> Parser::Parse()
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
            auto it = blocks_.emplace(str_, nullptr);
            if (it.second) {
              // Block not declared yet - backward jump target.
              block_ = new Block(str_);
              it.first->second = block_;
            } else {
              // Block was created by a forward jump.
              block_ = it.first->second;
            }
            topo_.push_back(block_);
          } else {
            // Start a new function.
            if (func_) EndFunction();
            funcName_ = str_;
          }
        } else {
          // New atom in a data segment.
          atom_ = new Atom(ParseName(str_));
          atom_->SetAlignment(llvm::Align(dataAlign_ ? *dataAlign_ : 1));
          dataAlign_ = std::nullopt;
          GetObject()->AddAtom(atom_);
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
        ParserError("unexpected token, expected operation");
      }
    }
  }

  if (func_) EndFunction();

  // Fix up function visibility attributes.
  for (std::string_view name : globls_) {
    if (auto *g = prog_->GetGlobalOrExtern(name)) {
      g->SetVisibility(Visibility::EXTERN);
    }
  }
  for (std::string_view name : weak_) {
    if (auto *g = prog_->GetGlobalOrExtern(name)) {
      g->SetVisibility(Visibility::WEAK);
    }
  }
  for (std::string_view name : export_) {
    if (auto *g = prog_->GetGlobalOrExtern(name)) {
      g->SetExported(true);
    }
  }

  return std::move(prog_);
}

// -----------------------------------------------------------------------------
void Parser::ParseQuad()
{
  if (!data_) {
    ParserError(".quad not in data segment");
  }
  switch (tk_) {
    case Token::MINUS: {
      NextToken();
      Check(Token::NUMBER);
      int64_t value = -int_;
      NextToken();
      return GetAtom()->AddItem(new Item(value));
    }
    case Token::NUMBER: {
      int64_t value = int_;
      NextToken();
      return GetAtom()->AddItem(new Item(value));
    }
    case Token::IDENT: {
      std::string name(ParseName(str_));
      if (name[0] == '.') {
        NextToken();
        GetAtom()->AddItem(new Item(
          new SymbolOffsetExpr(prog_->GetGlobalOrExtern(name), 0)
        ));
        return;
      } else {
        int64_t offset = 0;
        switch (NextToken()) {
          case Token::PLUS: {
            Expect(Token::NUMBER);
            offset = +int_;
            NextToken();
            break;
          }
          case Token::MINUS: {
            Expect(Token::NUMBER);
            offset = -int_;
            NextToken();
            break;
          }
          default: {
            break;
          }
        }
        return GetAtom()->AddItem(new Item(
          new SymbolOffsetExpr(prog_->GetGlobalOrExtern(name), offset)
        ));
      }
    }
    default: {
      ParserError("unexpected token, expected value");
    }
  }
}

// -----------------------------------------------------------------------------
void Parser::ParseComm()
{
  Check(Token::IDENT);
  std::string name = str_;
  Expect(Token::COMMA);
  Expect(Token::NUMBER);
  int64_t size = int_;
  Expect(Token::COMMA);
  Expect(Token::NUMBER);
  int64_t align = int_;
  Expect(Token::NEWLINE);

  if ((align & (align - 1)) != 0) {
    ParserError("Alignment not a power of two.");
  }

  if (func_) EndFunction();
  data_ = prog_->GetOrCreateData(".data");

  Atom *atom = new Atom(ParseName(name));
  atom->SetAlignment(llvm::Align(align));
  atom->AddItem(new Item(Item::Space{ static_cast<unsigned>(size) }));
  atom->SetVisibility(Visibility::WEAK);

  Object *object = new Object();
  object->AddAtom(atom);

  data_->AddObject(object);

  atom_ = nullptr;
  object_ = nullptr;
  dataAlign_ = std::nullopt;
}

// -----------------------------------------------------------------------------
void Parser::ParseInt8()
{
  GetAtom()->AddItem(new Item(static_cast<int8_t>(Number())));
}

// -----------------------------------------------------------------------------
void Parser::ParseInt16()
{
  GetAtom()->AddItem(new Item(static_cast<int16_t>(Number())));
}

// -----------------------------------------------------------------------------
void Parser::ParseInt32()
{
  GetAtom()->AddItem(new Item(static_cast<int32_t>(Number())));
}

// -----------------------------------------------------------------------------
void Parser::ParseDouble()
{
  union U { double f; int64_t i; } u = { .i = Number() };
  GetAtom()->AddItem(new Item(u.f));
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
      if (op == ".args") return ParseArgs();
      if (op == ".addrsig") return ParseAddrsig();
      if (op == ".addrsig_sym") return ParseAddrsigSym();
      break;
    }
    case 'b': {
      if (op == ".bss") return ParseBss();
      if (op == ".byte") return ParseInt8();
      break;
    }
    case 'c': {
      if (op == ".call") return ParseCall();
      if (op == ".comm") return ParseComm();
      break;
    }
    case 'd': {
      if (op == ".data") return ParseData();
      if (op == ".double") return ParseDouble();
      break;
    }
    case 'e': {
      if (op == ".end") return ParseEnd();
      break;
    }
    case 'f': {
      if (op == ".file") return ParseFile();
      break;
    }
    case 'g': {
      if (op == ".globl") return ParseGlobl();
      break;
    }
    case 'h': {
      if (op == ".hidden") return ParseHidden();
      break;
    }
    case 'i': {
      if (op == ".ident") return ParseIdent();
      break;
    }
    case 'l': {
      if (op == ".long") return ParseInt32();
      if (op == ".local") return ParseLocal();
      break;
    }
    case 'n': {
      if (op == ".noinline") return ParseNoInline();
      if (op == ".no_dead_strip") return ParseNoDeadStrip();
      break;
    }
    case 'p': {
      if (op == ".p2align") return ParseP2Align();
      break;
    }
    case 'q': {
      if (op == ".quad") return ParseQuad();
      break;
    }
    case 's': {
      if (op == ".short") return ParseInt16();
      if (op == ".space") return ParseSpace();
      if (op == ".stack_object") return ParseStackObject();
      if (op == ".section") return ParseSection();
      if (op == ".set") return ParseSet();
      break;
    }
    case 't': {
      if (op == ".text") return ParseText();
      break;
    }
    case 'v': {
      if (op == ".vararg") { return ParseVararg(); }
      if (op == ".visibility") return ParseVisibility();
      break;
    }
    case 'w': {
      if (op == ".weak") return ParseWeak();
      break;
    }
  }

  ParserError("unknown directive: " + op);
}

// -----------------------------------------------------------------------------
void Parser::ParseInstruction()
{
  // Make sure instruction is in text.
  InFunc();

  // Make sure we have a correct function.
  Func *func = GetFunction();

  // An instruction is composed of an opcode, followed by optional annotations.
  size_t dot = str_.find('.');
  std::string op = str_.substr(0, dot);

  std::optional<size_t> size;
  std::optional<Cond> cc;
  std::vector<Type> types;
  std::optional<CallingConv> conv;

  // Parse the tokens composing the opcode - size, condition code and types.
  while (dot != std::string::npos) {
    // Split the string at the next dot.
    size_t next = str_.find('.', dot + 1);
    size_t length = next == std::string::npos ? next : (next - dot - 1);
    std::string_view token = std::string_view(str_).substr(dot + 1, length);
    if (length == 0) {
      ParserError("invalid opcode " + str_);
    }
    dot = next;

    switch (token[0]) {
      case 'e': {
        if (token == "eq") { cc = Cond::EQ; continue; }
        break;
      }
      case 'l': {
        if (token == "lt") { cc = Cond::LT; continue; }
        if (token == "le") { cc = Cond::LE; continue; }
        break;
      }
      case 'g': {
        if (token == "gt") { cc = Cond::GT; continue; }
        if (token == "ge") { cc = Cond::GE; continue; }
        break;
      }
      case 'n': {
        if (token == "ne") { cc = Cond::NE; continue; }
        break;
      }
      case 'i': {
        if (token == "i8") { types.push_back(Type::I8); continue; }
        if (token == "i16") { types.push_back(Type::I16); continue; }
        if (token == "i32") { types.push_back(Type::I32); continue; }
        if (token == "i64") { types.push_back(Type::I64); continue; }
        if (token == "i128") { types.push_back(Type::I128); continue; }
        break;
      }
      case 'f': {
        if (token == "f32") { types.push_back(Type::F32); continue; }
        if (token == "f64") { types.push_back(Type::F64); continue; }
        if (token == "f80") { types.push_back(Type::F80); continue; }
        break;
      }
      case 'o': {
        if (token == "oeq") { cc = Cond::OEQ; continue; }
        if (token == "one") { cc = Cond::ONE; continue; }
        if (token == "olt") { cc = Cond::OLT; continue; }
        if (token == "ogt") { cc = Cond::OGT; continue; }
        if (token == "ole") { cc = Cond::OLE; continue; }
        if (token == "oge") { cc = Cond::OGE; continue; }
        break;
      }
      case 'u': {
        if (token == "ueq") { cc = Cond::UEQ; continue; }
        if (token == "une") { cc = Cond::UNE; continue; }
        if (token == "ult") { cc = Cond::ULT; continue; }
        if (token == "ugt") { cc = Cond::UGT; continue; }
        if (token == "ule") { cc = Cond::ULE; continue; }
        if (token == "uge") { cc = Cond::UGE; continue; }
        break;
      }
      default: {
        if (isdigit(token[0])) {
          // Parse integers, i.e. size operands.
          uint64_t sz = 0;
          for (size_t i = 0; i < token.size(); ++i) {
            if (!IsDigit(token[i])) {
              ParserError("invalid opcode " + str_);
            }
            sz = sz * 10 + ToInt(token[i]);
          }
          size = sz;
          continue;
        } else {
          break;
        }
      }
    }
    conv = ParseCallingConv(token);
  }

  // Parse all arguments.
  std::vector<Value *> ops;
  do {
    switch (NextToken()) {
      case Token::NEWLINE: {
        if (!ops.empty()) ParserError("expected argument");
        break;
      }
      // $sp, $fp
      case Token::REG: {
        ops.emplace_back(new ConstantReg(reg_));
        NextToken();
        break;
      }
      // $123
      case Token::VREG: {
        ops.emplace_back(reinterpret_cast<Inst *>((vreg_ << 1) | 1));
        NextToken();
        break;
      }
      // [$123] or [$sp]
      case Token::LBRACKET: {
        switch (NextToken()) {
          case Token::REG: {
            ops.emplace_back(new ConstantReg(reg_));
            break;
          }
          case Token::VREG: {
            ops.emplace_back(reinterpret_cast<Inst *>((vreg_ << 1) | 1));
            break;
          }
          default: {
            ParserError("invalid indirection");
          }
        }
        Expect(Token::RBRACKET);
        NextToken();
        break;
      }
      // -123
      case Token::MINUS: {
        Expect(Token::NUMBER);
        ops.emplace_back(new ConstantInt(-int_));
        NextToken();
        break;
      }
      // 123
      case Token::NUMBER: {
        ops.emplace_back(new ConstantInt(+int_));
        NextToken();
        break;
      }
      // _some_name + offset
      case Token::IDENT: {
        if (!str_.empty() && str_[0] == '.') {
          auto it = blocks_.emplace(str_, nullptr);
          if (it.second) {
            // Forward jump - create a placeholder block.
            it.first->second = new Block(str_);
          }
          ops.emplace_back(it.first->second);
          NextToken();
        } else {
          Global *global = prog_->GetGlobalOrExtern(ParseName(str_));
          switch (NextToken()) {
            case Token::PLUS: {
              Expect(Token::NUMBER);
              ops.emplace_back(new SymbolOffsetExpr(global, +int_));
              NextToken();
              break;
            }
            case Token::MINUS: {
              Expect(Token::NUMBER);
              ops.emplace_back(new SymbolOffsetExpr(global, -int_));
              NextToken();
              break;
            }
            default: {
              ops.emplace_back(global);
              break;
            }
          }
        }
        break;
      }
      default: {
        ParserError("invalid argument");
      }
    }
  } while (tk_ == Token::COMMA);

  // Parse optional annotations.
  AnnotSet annot;
  while (tk_ == Token::ANNOT) {
    if (str_ == "caml_frame") {
      NextToken();

      std::vector<size_t> allocs;
      bool raise;
      std::vector<CamlFrame::DebugInfos> infos;

      auto sexp = ParseSExp();
      if (auto *list = sexp.AsList()) {
        switch (list->size()) {
          case 0: break;
          case 3: {
            auto *sallocs = (*list)[0].AsList();
            auto *sraise = (*list)[1].AsNumber();
            auto *sinfos = (*list)[2].AsList();
            if (!sallocs || !sraise || !sinfos) {
              ParserError("invalid @caml_frame descriptor");
            }

            for (size_t i = 0; i < sallocs->size(); ++i) {
              if (auto *number = (*sallocs)[i].AsNumber()) {
                allocs.push_back(number->Get());
                continue;
              }
              ParserError("invalid allocation descriptor");
            }

            raise = sraise->Get();

            for (size_t i = 0; i < sinfos->size(); ++i) {
              if (auto *sinfo = (*sinfos)[i].AsList()) {
                CamlFrame::DebugInfos info;
                for (size_t j = 0; j < sinfo->size(); ++j) {
                  if (auto *sdebug = (*sinfo)[j].AsList()) {
                    if (sdebug->size() != 3) {
                      ParserError("malformed debug info descriptor");
                    }
                    auto *sloc = (*sdebug)[0].AsNumber();
                    auto *sfile = (*sdebug)[1].AsString();
                    auto *sdef = (*sdebug)[2].AsString();
                    if (!sloc || !sfile || !sdef) {
                      ParserError("missing debug info fields");
                    }

                    CamlFrame::DebugInfo debug;
                    debug.Location = sloc->Get();
                    debug.File = sfile->Get();
                    debug.Definition = sdef->Get();
                    info.push_back(std::move(debug));
                    continue;
                  }
                  ParserError("invalid debug info descriptor");
                }
                infos.push_back(std::move(info));
                continue;
              }
              ParserError("invalid debug infos descriptor");
            }
            break;
          }
          default: {
            ParserError("malformed @caml_frame descriptor");
          }
        }
      }

      if (!annot.Set<CamlFrame>(std::move(allocs), raise, std::move(infos))) {
        ParserError("duplicate @caml_frame");
      }
      continue;
    }
    if (str_ == "caml_value") {
      NextToken();
      if (!annot.Set<CamlValue>()) {
        ParserError("duplicate @caml_value");
      }
      continue;
    }
    if (str_ == "caml_addr") {
      NextToken();
      if (!annot.Set<CamlAddr>()) {
        ParserError("duplicate @caml_addr");
      }
      continue;
    }

    ParserError("invalid annotation");
  }

  // Done, must end with newline.
  Check(Token::NEWLINE);

  // Create a block for the instruction.
  if (block_ == nullptr) {
    // An empty start block, if not explicitly defined.
    block_ = new Block(".LBBentry" + std::to_string(++nextLabel_));
    topo_.push_back(block_);
  } else if (!block_->empty()) {
    // If the previous instruction is a terminator, start a new block.
    Inst *l = &*block_->rbegin();
    if (l->IsTerminator()) {
      block_ = new Block(".LBBterm" + std::to_string(++nextLabel_));
      topo_.push_back(block_);
    }
  }

  // Add the instruction to the block.
  Inst *i = CreateInst(op, ops, cc, size, types, conv, std::move(annot));
  for (unsigned idx = 0, rets = i->GetNumRets(); idx < rets; ++idx) {
    const auto vreg = reinterpret_cast<uint64_t>(ops[idx]);
    vregs_[i] = vreg >> 1;
  }

  block_->AddInst(i);
}

// -----------------------------------------------------------------------------
void Parser::ParseData()
{
  if (func_) EndFunction();

  atom_ = nullptr;
  object_ = nullptr;

  switch (tk_) {
    case Token::IDENT: {
      data_ = prog_->GetOrCreateData(str_);
      Expect(Token::NEWLINE);
      break;
    }
    case Token::NEWLINE: {
      data_ = prog_->GetOrCreateData(".data");
      break;
    }
    default: {
      ParserError("expected newline or identifier");
      break;
    }
  }
}


// -----------------------------------------------------------------------------
void Parser::ParseText()
{
  if (func_) EndFunction();
  data_ = nullptr;
  object_ = nullptr;
  Check(Token::NEWLINE);
}

// -----------------------------------------------------------------------------
void Parser::ParseBss()
{
  if (func_) EndFunction();

  atom_ = nullptr;
  object_ = nullptr;
  data_ = prog_->GetOrCreateData(".bss");
  Check(Token::NEWLINE);
}

// -----------------------------------------------------------------------------
void Parser::ParseSection()
{
  if (func_) EndFunction();

  atom_ = nullptr;
  object_ = nullptr;

  std::string name;
  switch (tk_) {
    case Token::STRING:
    case Token::IDENT: {
      name = str_;
      break;
    }
    default: {
      ParserError("expected string or ident");
    }
  }

  switch (NextToken()) {
    case Token::NEWLINE: {
      break;
    }
    case Token::COMMA: {
      Expect(Token::STRING);
      Expect(Token::COMMA);
      Expect(Token::ANNOT);
      switch (NextToken()) {
        case Token::COMMA: {
          Expect(Token::NUMBER);
          Expect(Token::NEWLINE);
          break;
        }
        case Token::NEWLINE: {
          break;
        }
        default: {
          ParserError("expected comma or newline");
        }
      }
      break;
    }
    default: {
      ParserError("expected newline or comma");
    }
  }
  if (name.substr(0, 5) == ".text") {
    if (func_) EndFunction();
    data_ = nullptr;
    object_ = nullptr;
  } else {
    data_ = prog_->GetOrCreateData(name);
  }
}

// -----------------------------------------------------------------------------
Inst *Parser::CreateInst(
    const std::string &opc,
    const std::vector<Value *> &ops,
    const std::optional<Cond> &ccs,
    const std::optional<size_t> &size,
    const std::vector<Type> &ts,
    const std::optional<CallingConv> &conv,
    AnnotSet &&annot)
{
  auto val = [this, &ops](int idx) {
    if ((idx < 0 && -idx > ops.size()) || (idx >= 0 && idx >= ops.size())) {
      ParserError("Missing operand");
    }
    return idx >= 0 ? ops[idx] : *(ops.end() + idx);
  };
  auto t = [this, &ts](int idx) {
    if ((idx < 0 && -idx > ts.size()) || (idx >= 0 && idx >= ts.size())) {
      ParserError("Missing type");
    }
    return idx >= 0 ? ts[idx] : *(ts.end() + idx);
  };
  auto op = [this, &val](int idx) {
    Value *v = val(idx);
    if ((reinterpret_cast<uintptr_t>(v) & 1) == 0) {
      ParserError("vreg expected");
    }
    return static_cast<Inst *>(v);
  };
  auto is_bb = [this, &val](int idx) {
    Value *v = val(idx);
    if ((reinterpret_cast<uintptr_t>(v) & 1) != 0 || !v->Is(Value::Kind::GLOBAL)) {
      return false;
    }
    auto *b = static_cast<Global *>(v);
    return b->Is(Global::Kind::BLOCK);
  };
  auto bb = [this, &val, &is_bb](int idx) {
    if (!is_bb(idx)) {
      ParserError("not a block");
    }
    return static_cast<Block *>(val(idx));
  };
  auto imm = [&val](int idx) {
    return static_cast<ConstantInt *>(val(idx));
  };
  auto reg = [&val](int idx) {
    return static_cast<ConstantReg *>(val(idx));
  };
  auto cc = [&ccs]() { return *ccs; };
  auto sz = [&size]() { return *size; };
  auto call = [this, &conv]() {
    if (!conv) {
      ParserError("missing calling conv");
    }
    return *conv;
  };
  auto args = [this, &ops](int beg, int end) {
    std::vector<Inst *> args;
    for (auto it = ops.begin() + beg; it != ops.end() + end; ++it) {
      if ((reinterpret_cast<uintptr_t>(*it) & 1) == 0) {
        ParserError("vreg expected");
      }
      args.push_back(static_cast<Inst *>(*it));
    }
    return args;
  };

  assert(opc.size() > 0 && "empty token");
  switch (opc[0]) {
    case 'a': {
      if (opc == "abs")  return new AbsInst(t(0), op(1), std::move(annot));
      if (opc == "add")  return new AddInst(t(0), op(1), op(2), std::move(annot));
      if (opc == "and")  return new AndInst(t(0), op(1), op(2), std::move(annot));
      if (opc == "arg")  return new ArgInst(t(0), imm(1), std::move(annot));
      if (opc == "alloca") return new AllocaInst(t(0), op(1), imm(2), std::move(annot));
      break;
    }
    case 'c': {
      if (opc == "cmp")  return new CmpInst(t(0), cc(), op(1), op(2), std::move(annot));
      if (opc == "cmpxchg") return new CmpXchgInst(t(0), op(1), op(2), op(3), std::move(annot));
      if (opc == "cos")  return new CosInst(t(0), op(1), std::move(annot));
      if (opc == "copysign") return new CopySignInst(t(0), op(1), op(2), std::move(annot));
      if (opc == "call") {
        if (ts.empty()) {
          return new CallInst(
              op(0),
              args(1, 0),
              size.value_or(ops.size() - 1),
              call(),
              std::move(annot)
          );
        } else {
          return new CallInst(
              t(0),
              op(1),
              args(2, 0),
              size.value_or(ops.size() - 2),
              call(),
              std::move(annot)
          );
        }
      }
      if (opc == "clz")  return new CLZInst(t(0), op(1), std::move(annot));
      if (opc == "ctz")  return new CTZInst(t(0), op(1), std::move(annot));
      break;
    }
    case 'e': {
      if (opc == "exp") return new ExpInst(t(0), op(1), std::move(annot));
      if (opc == "exp2") return new Exp2Inst(t(0), op(1), std::move(annot));
      break;
    }
    case 'i': {
      if (opc == "invoke") {
        if (ts.empty()) {
          if (is_bb(-2)) {
            return new InvokeInst(
                op(0),
                args(1, -2),
                bb(-2),
                bb(-1),
                ops.size() - 3,
                call(),
                std::move(annot)
            );
          } else {
            return new InvokeInst(
                op(0),
                args(1, -1),
                nullptr,
                bb(-1),
                ops.size() - 2,
                call(),
                std::move(annot)
            );
          }
        } else {
          if (is_bb(-2)) {
            return new InvokeInst(
                t(0),
                op(1),
                args(2, -2),
                bb(-2),
                bb(-1),
                ops.size() - 4,
                call(),
                std::move(annot)
            );
          } else {
            return new InvokeInst(
                t(0),
                op(1),
                args(2, -1),
                nullptr,
                bb(-1),
                ops.size() - 3,
                call(),
                std::move(annot)
            );
          }
        }
      }
      break;
    }
    case 'f': {
      if (opc == "fext")   return new FExtInst(t(0), op(1), std::move(annot));
      if (opc == "frame")  return new FrameInst(t(0), imm(1), imm(2), std::move(annot));
      if (opc == "fceil") return new FCeilInst(t(0), op(1), std::move(annot));
      if (opc == "ffloor") return new FFloorInst(t(0), op(1), std::move(annot));
      if (opc == "fnstcw") return new FNStCwInst(op(0), std::move(annot));
      if (opc == "fldcw") return new FLdCwInst(op(0), std::move(annot));
      break;
    }
    case 'j': {
      if (opc == "jf")  return new JumpCondInst(op(0), nullptr, bb(1), std::move(annot));
      if (opc == "jt")  return new JumpCondInst(op(0), bb(1), nullptr, std::move(annot));
      if (opc == "ji")  return new JumpIndirectInst(op(0), std::move(annot));
      if (opc == "jmp") return new JumpInst(bb(0), std::move(annot));
      if (opc == "jcc") return new JumpCondInst(op(0), bb(1), bb(2), std::move(annot));
      break;
    }
    case 'l': {
      if (opc == "ld") return new LoadInst(t(0), op(1), std::move(annot));
      if (opc == "log") return new LogInst(t(0), op(1), std::move(annot));
      if (opc == "log2") return new Log2Inst(t(0), op(1), std::move(annot));
      if (opc == "log10") return new Log10Inst(t(0), op(1), std::move(annot));
      break;
    }
    case 'm': {
      if (opc == "mov") return new MovInst(t(0), val(1), std::move(annot));
      if (opc == "mul") return new MulInst(t(0), op(1), op(2), std::move(annot));
      break;
    }
    case 'n': {
      if (opc == "neg") return new NegInst(t(0), op(1), std::move(annot));
      break;
    }
    case 'o': {
      if (opc == "or") return new OrInst(t(0), op(1), op(2), std::move(annot));
      break;
    }
    case 'p': {
      if (opc == "pow")  return new PowInst(t(0), op(1), op(2), std::move(annot));
      if (opc == "phi") {
        if ((ops.size() & 1) == 0) {
          ParserError("Invalid PHI instruction");
        }
        PhiInst *phi = new PhiInst(t(0), std::move(annot));
        for (unsigned i = 1; i < ops.size(); i += 2) {
          auto op = ops[i + 1];
          if ((reinterpret_cast<uintptr_t>(op) & 1) == 0) {
            ParserError("vreg expected");
          }
          phi->Add(bb(i), static_cast<Inst *>(op));
        }
        return phi;
      }
      if (opc == "popcnt")  return new PopCountInst(t(0), op(1), std::move(annot));
      break;
    }
    case 'r': {
      if (opc == "rotl")  return new RotlInst(t(0), op(1), op(2), std::move(annot));
      if (opc == "rotr")  return new RotrInst(t(0), op(1), op(2), std::move(annot));
      if (opc == "rdtsc") return new RdtscInst(t(0), std::move(annot));
      if (opc == "ret") {
        if (ops.empty()) {
          return new ReturnInst(std::move(annot));
        } else {
          return new ReturnInst(op(0), std::move(annot));
        }
      }
      break;
    }
    case 's': {
      if (opc == "syscall") {
        return new SyscallInst(t(0), op(1), args(2, 0), std::move(annot));
      }
      if (opc == "sdiv")  return new SDivInst(t(0), op(1), op(2), std::move(annot));
      if (opc == "srem")  return new SRemInst(t(0), op(1), op(2), std::move(annot));
      if (opc == "saddo") return new AddSOInst(t(0), op(1), op(2), std::move(annot));
      if (opc == "smulo") return new MulSOInst(t(0), op(1), op(2), std::move(annot));
      if (opc == "ssubo") return new SubSOInst(t(0), op(1), op(2), std::move(annot));
      if (opc == "set")   return new SetInst(reg(0), op(1), std::move(annot));
      if (opc == "sext")  return new SExtInst(t(0), op(1), std::move(annot));
      if (opc == "sll")   return new SllInst(t(0), op(1), op(2), std::move(annot));
      if (opc == "sra")   return new SraInst(t(0), op(1), op(2), std::move(annot));
      if (opc == "srl")   return new SrlInst(t(0), op(1), op(2), std::move(annot));
      if (opc == "st")    return new StoreInst(op(0), op(1), std::move(annot));
      if (opc == "sub")   return new SubInst(t(0), op(1), op(2), std::move(annot));
      if (opc == "sqrt")  return new SqrtInst(t(0), op(1), std::move(annot));
      if (opc == "sin")   return new SinInst(t(0), op(1), std::move(annot));
      if (opc == "select") {
        return new SelectInst(t(0), op(1), op(2), op(3), std::move(annot));
      }
      if (opc == "switch") {
        std::vector<Block *> blocks;
        for (auto it = ops.begin() + 1; it != ops.end(); ++it) {
          blocks.push_back(static_cast<Block *>(*it));
        }
        return new SwitchInst(op(0), blocks, std::move(annot));
      }
      break;
    }
    case 't': {
      if (opc == "trunc") return new TruncInst(t(0), op(1), std::move(annot));
      if (opc == "trap")  return new TrapInst(std::move(annot));
      if (opc == "tcall") {
        if (ts.empty()) {
          return new TailCallInst(
              op(0),
              args(1, 0),
              size.value_or(ops.size() - 1),
              call(),
              std::move(annot)
          );
        } else {
          return new TailCallInst(
              t(0),
              op(0),
              args(1, 0),
              size.value_or(ops.size() - 1),
              call(),
              std::move(annot)
          );
        }
      }
      if (opc == "tinvoke") {
        if (ts.empty()) {
          return new TailInvokeInst(
              op(0),
              args(1, -1),
              bb(-1),
              size.value_or(ops.size() - 2),
              call(),
              std::move(annot)
          );
        } else {
          return new TailInvokeInst(
              t(0),
              op(0),
              args(1, -1),
              bb(-1),
              size.value_or(ops.size() - 2),
              call(),
              std::move(annot)
          );
        }
      }
      break;
    }
    case 'u': {
      if (opc == "udiv") return new UDivInst(t(0), op(1), op(2), std::move(annot));
      if (opc == "urem") return new URemInst(t(0), op(1), op(2), std::move(annot));
      if (opc == "uaddo") return new AddUOInst(t(0), op(1), op(2), std::move(annot));
      if (opc == "umulo") return new MulUOInst(t(0), op(1), op(2), std::move(annot));
      if (opc == "usubo") return new SubUOInst(t(0), op(1), op(2), std::move(annot));
      if (opc == "undef") return new UndefInst(t(0), std::move(annot));
      break;
    }
    case 'v': {
      if (opc == "vastart") return new VAStartInst(op(0), std::move(annot));
      break;
    }
    case 'x': {
      if (opc == "xext")   return new XExtInst(t(0), op(1), std::move(annot));
      if (opc == "xchg") return new XchgInst(t(0), op(1), op(2), std::move(annot));
      if (opc == "xor")  return new XorInst(t(0), op(1), op(2), std::move(annot));
      break;
    }
    case 'z': {
      if (opc == "zext") return new ZExtInst(t(0), op(1), std::move(annot));
      break;
    }
  }

  ParserError("unknown opcode: " + opc);
}

// -----------------------------------------------------------------------------
Object *Parser::GetObject()
{
  if (object_) {
    return object_;
  }

  object_ = new Object();
  data_->AddObject(object_);
  return object_;
}

// -----------------------------------------------------------------------------
Atom *Parser::GetAtom()
{
  if (!atom_) {
    object_ = new Object();
    data_->AddObject(object_);

    atom_ = new Atom((data_->getName() + "$begin").str());
    object_->AddAtom(atom_);

    if (dataAlign_) {
      atom_->SetAlignment(llvm::Align(*dataAlign_));
      dataAlign_ = std::nullopt;
    }
  } else {
    if (dataAlign_) {
      atom_->AddItem(new Item(Item::Align{ *dataAlign_ }));
      dataAlign_ = std::nullopt;
    }
  }

  return atom_;
}

// -----------------------------------------------------------------------------
Func *Parser::GetFunction()
{
  if (!func_) {
    func_ = new Func(ParseName(*funcName_));
    prog_->AddFunc(func_);
    if (funcAlign_) {
      func_->SetAlignment(llvm::Align(*funcAlign_));
      funcAlign_ = std::nullopt;
    }
  }
  return func_;
}

// -----------------------------------------------------------------------------
void Parser::EndFunction()
{
  // Add the blocks to the function, in order. Add jumps to blocks which
  // fall through and fix the fall-through branches of conditionals.
  for (auto it = topo_.begin(); it != topo_.end(); ++it) {
    Block *block = *it;
    if (auto term = block->GetTerminator()) {
      for (Use &use : term->operands()) {
        if (use == nullptr) {
          if (it + 1 == topo_.end()) {
            ParserError(func_, "Jump falls through");
          } else {
            use = *(it + 1);
          }
        }
      }
    } else if (it + 1 != topo_.end()) {
      block->AddInst(new JumpInst(*(it + 1), {}));
    } else {
      ParserError(func_, "Unterminated function");
    }
    func_->AddBlock(block);
  }

  // Check if function is ill-defined.
  if (func_->empty()) {
    ParserError(func_, "Empty function");
  }

  PhiPlacement();

  func_ = nullptr;
  funcName_ = std::nullopt;
  block_ = nullptr;

  vregs_.clear();
  blocks_.clear();
  topo_.clear();
}

// -----------------------------------------------------------------------------
void Parser::PhiPlacement()
{
  // Construct the dominator tree & find dominance frontiers.
  DominatorTree DT(*func_);
  DominanceFrontier DF;
  DF.analyze(DT);

  // Find all definitions of all variables.
  llvm::DenseSet<unsigned> custom;
  for (Block &block : *func_) {
    for (PhiInst &inst : block.phis()) {
      for (Use &use : inst.operands()) {
        const auto vreg = reinterpret_cast<uint64_t>(use.get());
        if (vreg & 1) {
          custom.insert(vreg >> 1);
        }
      }
    }
  }

  llvm::DenseMap<unsigned, std::queue<Inst *>> sites;
  for (Block &block : *func_) {
    llvm::DenseMap<unsigned, Inst *> localSites;
    for (Inst &inst : block) {
      if (auto it = vregs_.find(&inst); it != vregs_.end()) {
        unsigned vreg = it->second;
        if (inst.GetNumRets() > 0 && custom.count(vreg) == 0) {
          localSites[vreg] = &inst;
        }
      }
    }
    for (const auto &site : localSites) {
      sites[site.first].push(site.second);
    }
  }

  // Find the dominance frontier of the blocks where variables are defined.
  // Place PHI nodes at the start of those blocks, continuing with the
  // dominance frontier of those nodes iteratively.
  for (auto &var : sites) {
    auto &q = var.second;
    while (!q.empty()) {
      auto *inst = q.front();
      q.pop();
      auto *block = inst->getParent();
      if (auto *node = DT.getNode(block)) {
        for (auto &front : DF.calculate(DT, node)) {
          bool found = false;
          for (PhiInst &phi : front->phis()) {
            if (auto it = vregs_.find(&phi); it != vregs_.end()) {
              if (it->second == var.first) {
                found = true;
                break;
              }
            }
          }

          // If the PHI node was not added already, add it.
          if (!found) {
            auto *phi = new PhiInst(inst->GetType(0), {});
            front->AddPhi(phi);
            vregs_[phi] = var.first;
            q.push(phi);
          }
        }
      }
    }
  }

  // Renaming variables to point to definitions or PHI nodes.
  llvm::DenseMap<unsigned, std::stack<Inst *>> vars;
  llvm::SmallPtrSet<Block *, 8> blocks;
  std::function<void(Block *block)> rename = [&](Block *block) {
    // Add the block to the set of visited ones.
    blocks.insert(block);

    // Register the names of incoming PHIs.
    for (PhiInst &phi : block->phis()) {
      auto it = vregs_.find(&phi);
      if (it != vregs_.end()) {
        vars[it->second].push(&phi);
      }
    }

    // Rename all non-phis, registering them in the map.
    for (Inst &inst : *block) {
      if (inst.Is(Inst::Kind::PHI)) {
        continue;
      }

      for (Use &use : inst.operands()) {
        const auto vreg = reinterpret_cast<uint64_t>(use.get());
        if (vreg & 1) {
          auto &stk = vars[vreg >> 1];
          if (stk.empty()) {
            ParserError(
                func_,
                block,
                "undefined vreg: " + std::to_string(vreg >> 1)
            );
          }
          use = stk.top();
        }
      }

      if (auto it = vregs_.find(&inst); it != vregs_.end()) {
        vars[it->second].push(&inst);
      }
    }

    // Handle PHI nodes in successors.
    for (Block *succ : block->successors()) {
      for (PhiInst &phi : succ->phis()) {
        auto &stk = vars[vregs_[&phi]];
        if (!stk.empty()) {
          phi.Add(block, stk.top());
        } else if (!phi.HasValue(block)) {
          Type type = phi.GetType();
          UndefInst *undef = nullptr;
          for (auto it = block->rbegin(); it != block->rend(); ++it) {
            if (it->Is(Inst::Kind::UNDEF)) {
              UndefInst *inst = static_cast<UndefInst *>(&*it);
              if (inst->GetType() == type) {
                undef = inst;
                break;
              }
            }
          }
          if (!undef) {
            undef = new UndefInst(phi.GetType(), {});
            block->AddInst(undef, block->GetTerminator());
          }
          phi.Add(block, undef);
        } else {
          auto *value = phi.GetValue(block);
          const auto vreg = reinterpret_cast<uint64_t>(value);
          if (vreg & 1) {
            phi.Add(block, vars[vreg >> 1].top());
          }
        }
      }
    }

    // Recursively rename child nodes.
    for (const auto *child : *DT[block]) {
      rename(child->getBlock());
    }

    // Pop definitions of this block from the stack.
    for (auto it = block->rbegin(); it != block->rend(); ++it) {
      if (auto jt = vregs_.find(&*it); jt != vregs_.end()) {
        auto &q = vars[jt->second];
        assert(q.top() == &*it && "invalid type");
        q.pop();
      }
    }
  };
  rename(DT.getRoot());

  // Remove blocks which are trivially dead.
  std::vector<PhiInst *> queue;
  for (auto it = func_->begin(); it != func_->end(); ) {
    Block *block = &*it++;
    if (blocks.count(block) == 0) {
      block->replaceAllUsesWith(new ConstantInt(0));
      block->eraseFromParent();
    } else {
      for (auto &phi : block->phis()) {
        queue.push_back(&phi);
      }
    }
  }

  // Fix up annotations for PHIs: decide between address and value.
  while (!queue.empty()) {
    PhiInst *phi = queue.back();
    queue.pop_back();

    bool isValue = false;
    bool isAddr = false;
    for (unsigned i = 0, n = phi->GetNumIncoming(); i < n; ++i) {
      if (auto *inst = ::dyn_cast_or_null<Inst>(phi->GetValue(i))) {
        isValue = isValue || inst->HasAnnot<CamlValue>();
        isAddr = isAddr || inst->HasAnnot<CamlAddr>();
      }
    }

    bool changed = false;
    if (!phi->HasAnnot<CamlAddr>() && isAddr) {
      phi->ClearAnnot<CamlValue>();
      phi->SetAnnot<CamlAddr>();
      changed = true;
    }
    if (!phi->HasAnnot<CamlValue>() && isValue) {
      phi->SetAnnot<CamlValue>();
      changed = true;
    }

    if (changed) {
      for (auto *user : phi->users()) {
        if (auto *phiUser = ::dyn_cast_or_null<PhiInst>(user)) {
          queue.push_back(phiUser);
        }
      }
    }
  }

  for (Block &block : *func_) {
    for (auto it = block.begin(); it != block.end(); ) {
      if (auto *phi = ::dyn_cast_or_null<PhiInst>(&*it++)) {
        // Remove redundant PHIs.
        llvm::SmallPtrSet<PhiInst *, 10> phiCycle;

        std::function<bool(PhiInst *)> isDeadCycle = [&] (PhiInst *phi)  -> bool
        {
          if (!phiCycle.insert(phi).second) {
            return true;
          }

          for (User *user : phi->users()) {
            if (auto *nextPhi = ::dyn_cast_or_null<PhiInst>(user)) {
              if (!isDeadCycle(nextPhi)) {
                return false;
              }
              continue;
            }
            return false;
          }
          return true;
        };

        if (isDeadCycle(phi)) {
          for (PhiInst *deadPhi : phiCycle) {
            if (deadPhi == &*it) {
              ++it;
            }
            deadPhi->replaceAllUsesWith(nullptr);
            deadPhi->eraseFromParent();
          }
        }
      } else {
        break;
      }
    }
  }
}

// -----------------------------------------------------------------------------
void Parser::ParseAlign()
{
  Check(Token::NUMBER);
  if ((int_ & (int_ - 1)) != 0) {
    ParserError("Alignment not a power of two.");
  }

  if (int_ > std::numeric_limits<uint8_t>::max()) {
    ParserError("Alignment out of bounds");
  }

  if (data_) {
    dataAlign_ = int_;
  } else {
    if (func_) {
      EndFunction();
    }
    funcAlign_ = int_;
  }
  Expect(Token::NEWLINE);
}

// -----------------------------------------------------------------------------
void Parser::ParseP2Align()
{
  Check(Token::NUMBER);
  if (int_ > CHAR_BIT) {
    ParserError("Alignment out of bounds");
  }
  if (data_) {
    dataAlign_ = 1u << int_;
  } else {
    if (func_) {
      EndFunction();
    }
    funcAlign_ = 1u << int_;
  }
  Expect(Token::NEWLINE);
}

// -----------------------------------------------------------------------------
void Parser::ParseEnd()
{
  if (!func_ && !funcName_) {
    object_ = nullptr;
    atom_ = nullptr;
  }
  Check(Token::NEWLINE);
}

// -----------------------------------------------------------------------------
void Parser::ParseSpace()
{
  Check(Token::NUMBER);
  unsigned length = int_;
  InData();
  Atom *atom = GetAtom();
  switch (NextToken()) {
    case Token::NEWLINE: {
      atom->AddItem(new Item(Item::Space{ length }));
      break;
    }
    case Token::COMMA: {
      Expect(Token::NUMBER);
      if (int_ == 0) {
        atom->AddItem(new Item(Item::Space{ length }));
      } else {
        for (unsigned i = 0; i < length; ++i) {
          atom->AddItem(new Item(static_cast<int8_t>(int_)));
        }
      }
      Expect(Token::NEWLINE);
      break;
    }
    default: {
      ParserError("expected newline or comma");
    }
  }
}

// -----------------------------------------------------------------------------
void Parser::ParseStackObject()
{
  if (!funcName_) {
    ParserError("stack_object not in function");
  }

  Check(Token::NUMBER);
  unsigned index = int_;
  Expect(Token::COMMA);
  Expect(Token::NUMBER);
  unsigned size = int_;
  Expect(Token::COMMA);
  Expect(Token::NUMBER);
  unsigned align = int_;
  Expect(Token::NEWLINE);

  GetFunction()->AddStackObject(index, size, llvm::Align(align));
}

// -----------------------------------------------------------------------------
void Parser::ParseCall()
{
  Check(Token::IDENT);
  if (!funcName_) {
    ParserError("stack directive not in function");
  }
  GetFunction()->SetCallingConv(ParseCallingConv(str_));
  Expect(Token::NEWLINE);
}

// -----------------------------------------------------------------------------
void Parser::ParseArgs()
{
  InFunc();
  auto *func = GetFunction();

  if (tk_ == Token::IDENT) {
    std::vector<Type> types;
    do {
      Check(Token::IDENT);
      switch (str_[0]) {
        case 'i': {
          if (str_ == "i8") { types.push_back(Type::I8); continue; }
          if (str_ == "i16") { types.push_back(Type::I16); continue; }
          if (str_ == "i32") { types.push_back(Type::I32); continue; }
          if (str_ == "i64") { types.push_back(Type::I64); continue; }
          if (str_ == "i128") { types.push_back(Type::I128); continue; }
          break;
        }
        case 'f': {
          if (str_ == "f32") { types.push_back(Type::F32); continue; }
          if (str_ == "f64") { types.push_back(Type::F64); continue; }
          if (str_ == "f80") { types.push_back(Type::F80); continue; }
          break;
        }
        default: {
          break;
        }
      }
      ParserError("invalid type");
    } while (NextToken() == Token::COMMA && NextToken() == Token::IDENT);
    func_->SetParameters(types);
  }
  Check(Token::NEWLINE);
}

// -----------------------------------------------------------------------------
void Parser::ParseVararg()
{
  InFunc();
  GetFunction()->SetVarArg(true);
  Check(Token::NEWLINE);
}

// -----------------------------------------------------------------------------
void Parser::ParseVisibility()
{
  Check(Token::IDENT);
  auto vis = ParseVisibility(str_);
  if (atom_) {
    atom_->SetVisibility(vis);
  } else {
    if (!funcName_) {
      ParserError("stack directive not in function");
    }
    GetFunction()->SetVisibility(vis);
  }
  Expect(Token::NEWLINE);
}

// -----------------------------------------------------------------------------
void Parser::ParseNoInline()
{
  if (!funcName_) {
    ParserError("noinline directive not in function");
  }
  GetFunction()->SetNoInline(true);
  Check(Token::NEWLINE);
}

// -----------------------------------------------------------------------------
void Parser::ParseGlobl()
{
  Check(Token::IDENT);
  globls_.insert(str_);
  Expect(Token::NEWLINE);
}

// -----------------------------------------------------------------------------
void Parser::ParseNoDeadStrip()
{
  Check(Token::IDENT);
  export_.insert(str_);
  Expect(Token::NEWLINE);
}

// -----------------------------------------------------------------------------
void Parser::ParseHidden()
{
  Check(Token::IDENT);
  hidden_.insert(str_);
  Expect(Token::NEWLINE);
}

// -----------------------------------------------------------------------------
void Parser::ParseWeak()
{
  Check(Token::IDENT);
  weak_.insert(str_);
  Expect(Token::NEWLINE);
}

// -----------------------------------------------------------------------------
void Parser::ParseFile()
{
  Check(Token::STRING);
  Expect(Token::NEWLINE);
}

// -----------------------------------------------------------------------------
void Parser::ParseLocal()
{
  Check(Token::IDENT);
  Expect(Token::NEWLINE);
}

// -----------------------------------------------------------------------------
void Parser::ParseIdent()
{
  Check(Token::STRING);
  Expect(Token::NEWLINE);
}

// -----------------------------------------------------------------------------
void Parser::ParseSet()
{
  Check(Token::IDENT);
  auto *to = new Extern(str_);
  prog_->AddExtern(to);
  Expect(Token::COMMA);
  Expect(Token::IDENT);
  to->SetAlias(prog_->GetGlobalOrExtern(str_));
  Expect(Token::NEWLINE);
}

// -----------------------------------------------------------------------------
void Parser::ParseAddrsig()
{
  Check(Token::NEWLINE);
}

// -----------------------------------------------------------------------------
void Parser::ParseAddrsigSym()
{
  Check(Token::IDENT);
  Expect(Token::NEWLINE);
}

// -----------------------------------------------------------------------------
void Parser::ParseAscii()
{
  Check(Token::STRING);
  InData();
  GetAtom()->AddItem(new Item(str_));
  Expect(Token::NEWLINE);
}

// -----------------------------------------------------------------------------
void Parser::ParseAsciz()
{
  Check(Token::STRING);
  InData();
  Atom *atom = GetAtom();
  atom->AddItem(new Item(str_));
  atom->AddItem(new Item(static_cast<int8_t>(0)));
  Expect(Token::NEWLINE);
}

// -----------------------------------------------------------------------------
void Parser::InData()
{
  if (data_ == nullptr || func_ != nullptr) {
    ParserError("not in a data segment");
  }
}

// -----------------------------------------------------------------------------
void Parser::InFunc()
{
  if (data_ != nullptr || !funcName_) {
    ParserError("not in a text segment");
  }
}

// -----------------------------------------------------------------------------
CallingConv Parser::ParseCallingConv(const std::string_view str)
{
  return ParseToken<CallingConv>(kCallingConv, str);
}

// -----------------------------------------------------------------------------
Visibility Parser::ParseVisibility(const std::string_view str)
{
  return ParseToken<Visibility>(kVisibility, str);
}

// -----------------------------------------------------------------------------
int64_t Parser::Number()
{
  InData();
  int64_t val;
  if (tk_ == Token::MINUS) {
    Expect(Token::NUMBER);
    val = -int_;
  } else {
    Check(Token::NUMBER);
    val = int_;
  }
  Expect(Token::NEWLINE);
  return val;
}

// -----------------------------------------------------------------------------
SExp Parser::ParseSExp()
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
            ParserError("invalid token in s-expression");
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
    case '[': NextChar(); return tk_ = Token::LBRACKET;
    case ']': NextChar(); return tk_ = Token::RBRACKET;
    case '(': NextChar(); return tk_ = Token::LPAREN;
    case ')': NextChar(); return tk_ = Token::RPAREN;
    case ',': NextChar(); return tk_ = Token::COMMA;
    case '+': NextChar(); return tk_ = Token::PLUS;
    case '-': NextChar(); return tk_ = Token::MINUS;
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

        static std::array<std::pair<const char *, ConstantReg::Kind>, 20> regs =
        {
          std::make_pair("rax",        ConstantReg::Kind::RAX       ),
          std::make_pair("rbx",        ConstantReg::Kind::RBX       ),
          std::make_pair("rcx",        ConstantReg::Kind::RCX       ),
          std::make_pair("rdx",        ConstantReg::Kind::RDX       ),
          std::make_pair("rsi",        ConstantReg::Kind::RSI       ),
          std::make_pair("rdi",        ConstantReg::Kind::RDI       ),
          std::make_pair("rsp",        ConstantReg::Kind::RSP       ),
          std::make_pair("rbp",        ConstantReg::Kind::RBP       ),
          std::make_pair("r8",         ConstantReg::Kind::R8        ),
          std::make_pair("r9",         ConstantReg::Kind::R9        ),
          std::make_pair("r10",        ConstantReg::Kind::R10       ),
          std::make_pair("r11",        ConstantReg::Kind::R11       ),
          std::make_pair("r12",        ConstantReg::Kind::R12       ),
          std::make_pair("r13",        ConstantReg::Kind::R13       ),
          std::make_pair("r14",        ConstantReg::Kind::R14       ),
          std::make_pair("r15",        ConstantReg::Kind::R15       ),
          std::make_pair("fs",         ConstantReg::Kind::FS        ),
          std::make_pair("pc",         ConstantReg::Kind::PC        ),
          std::make_pair("ret_addr",   ConstantReg::Kind::RET_ADDR  ),
          std::make_pair("frame_addr", ConstantReg::Kind::FRAME_ADDR),
        };

        for (const auto &reg : regs) {
        if (reg.first == str_) {
            reg_ = reg.second;
            return tk_ = Token::REG;
          }
        }

        ParserError("unknown register: " + str_);
      } else {
        ParserError("invalid register name");
      }
    }
    case '@': {
      if (!IsAlphaNum(NextChar())) {
        ParserError("empty annotation");
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
                ParserError("invalid escape: " + std::string(1, char_));
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
                ParserError("invalid numeric constant");
              }
              return tk_ = Token::NUMBER;
            }
          }
        }
        do {
          int_ = int_ * base + ToInt(char_);
        } while (IsDigit(NextChar(), base));
        if (IsAlphaNum(char_)) {
          ParserError("invalid numeric constant");
        }
        return tk_ = Token::NUMBER;
      } else {
        ParserError("unexpected char: " + std::string(1, char_));
      }
    }
  }
}

// -----------------------------------------------------------------------------
char Parser::NextChar()
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
void Parser::Expect(Token type)
{
  NextToken();
  Check(type);
}

// -----------------------------------------------------------------------------
void Parser::Check(Token type)
{
  if (tk_ != type) {
    auto ToString = [](Token tk) -> std::string {
      switch (tk) {
        case Token::NEWLINE:  return "newline";
        case Token::END:      return "eof";
        case Token::LBRACKET:   return "'['";
        case Token::RBRACKET:   return "']'";
        case Token::LPAREN:   return "'('";
        case Token::RPAREN:   return "')'";
        case Token::COMMA:    return "','";
        case Token::REG:      return "reg";
        case Token::VREG:     return "vreg";
        case Token::IDENT:    return "identifier";
        case Token::LABEL:    return "label";
        case Token::NUMBER:   return "number";
        case Token::ANNOT:    return "annot";
        case Token::STRING:   return "string";
        case Token::PLUS:     return "'+'";
        case Token::MINUS:    return "'-'";
      }
      llvm_unreachable("invalid token");
    };
    ParserError(ToString(type) + " expected, got " + ToString(tk_));
  }
}

// -----------------------------------------------------------------------------
template<typename T>
T Parser::ParseToken(
    const std::vector<std::pair<const char *, T>> &options,
    const std::string_view str)
{
  bool valid = false;
  for (auto &flag : options) {
    if (flag.first == str) {
      return flag.second;
    }
  }
  ParserError("invalid token: " + std::string(str));
}

// -----------------------------------------------------------------------------
[[noreturn]] void Parser::ParserError(const std::string &msg)
{
  std::ostringstream os;
  os << "["
     << prog_->GetName() << ":" << row_ << ":" << col_
     << "]: " << msg;
  llvm::report_fatal_error(os.str());
}

// -----------------------------------------------------------------------------
[[noreturn]] void Parser::ParserError(Func *func, const std::string &msg)
{
  std::ostringstream os;
  os << "["
      << row_ << ":" << col_ << ": " << func->GetName()
     << "]: " << msg;
  llvm::report_fatal_error(os.str());
}

// -----------------------------------------------------------------------------
[[noreturn]] void Parser::ParserError(
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
