// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <cassert>
#include <array>
#include <optional>
#include <queue>
#include <stack>
#include <string_view>
#include <vector>

#include <llvm/ADT/SmallPtrSet.h>

#include "core/block.h"
#include "core/cast.h"
#include "core/constant.h"
#include "core/data.h"
#include "core/func.h"
#include "core/insts.h"
#include "core/parser.h"
#include "core/prog.h"



// -----------------------------------------------------------------------------
Parser::Parser(llvm::StringRef buf, std::string_view ident)
  : l_(buf)
  , prog_(new Prog(ident))
  , data_(nullptr)
  , atom_(nullptr)
  , func_(nullptr)
  , block_(nullptr)
  , nextLabel_(0)
{
}

// -----------------------------------------------------------------------------
Parser::~Parser()
{
}

// -----------------------------------------------------------------------------
std::unique_ptr<Prog> Parser::Parse()
{
  while (!l_.AtEnd()) {
    switch (l_.GetToken()) {
      case Token::NEWLINE: {
        l_.NextToken();
        continue;
      }
      case Token::LABEL: {
        std::string name(ParseName(l_.String()));
        if (data_ == nullptr) {
          if (func_) {
            // Start a new basic block.
            if (auto *g = prog_->GetGlobal(name)) {
              if (auto *ext = ::cast_or_null<Extern>(g)) {
                CreateBlock(name);
              } else {
                l_.Error(func_, "redefinition of '" + name + "'");
              }
            } else {
              CreateBlock(name);
            }
          } else {
            // Start a new function.
            func_ = new Func(name);
            prog_->AddFunc(func_);
            if (funcAlign_) {
              func_->SetAlignment(llvm::Align(*funcAlign_));
              funcAlign_ = std::nullopt;
            }
          }
        } else {
          // New atom in a data segment.
          atom_ = new Atom(name);
          atom_->SetAlignment(llvm::Align(dataAlign_ ? *dataAlign_ : 1));
          dataAlign_ = std::nullopt;
          GetObject()->AddAtom(atom_);
        }
        l_.Expect(Token::NEWLINE);
        continue;
      }
      case Token::IDENT: {
        auto name = l_.String();
        if (!name.empty() && name[0] == '.') {
          ParseDirective();
        } else {
          ParseInstruction();
        }
        l_.Check(Token::NEWLINE);
        continue;
      }
      default: {
        l_.Error("unexpected token, expected operation");
      }
    }
  }

  if (func_) EndFunction();

  // Fix up function visibility attributes.
  {
    // Gather all names.
    std::unordered_set<std::string> names;
    for (auto &attr : globls_) {
      names.insert(attr);
    }
    for (auto &attr : hidden_) {
      names.insert(attr);
    }
    for (auto &attr : weak_) {
      names.insert(attr);
    }

    // Coalesce visibility attributes.
    for (const auto &name : names) {
      std::optional<std::string> section;
      // Fetch individual flags.
      bool isGlobal = globls_.count(std::string(name));
      bool isHidden = hidden_.count(std::string(name));
      bool isWeak = weak_.count(std::string(name));

      // Build an attribute.
      Visibility vis;
      if (isGlobal) {
        vis = isHidden ? Visibility::GLOBAL_HIDDEN : Visibility::GLOBAL_DEFAULT;
      } else if (isWeak) {
        vis = isHidden ? Visibility::WEAK_HIDDEN : Visibility::WEAK_DEFAULT;
      } else {
        vis =  Visibility::LOCAL;
      }

      // Register the attribute.
      if (auto *g = prog_->GetGlobalOrExtern(name)) {
        g->SetVisibility(vis);
        if (auto *ext = ::cast_or_null<Extern>(g)) {
          if (section) {
            ext->SetSection(*section);
          }
        }
      }
    }
  }

  return std::move(prog_);
}

// -----------------------------------------------------------------------------
void Parser::ParseQuad()
{
  if (!data_) {
    l_.Error(".quad not in data segment");
  }
  switch (l_.GetToken()) {
    case Token::MINUS: {
      l_.Expect(Token::NUMBER);
      int64_t value = -l_.Int();
      l_.NextToken();
      return GetAtom()->AddItem(new Item(value));
    }
    case Token::NUMBER: {
      int64_t value = l_.Int();
      l_.NextToken();
      return GetAtom()->AddItem(new Item(value));
    }
    case Token::IDENT: {
      std::string name(ParseName(l_.String()));
      int64_t offset = 0;
      switch (l_.NextToken()) {
        case Token::PLUS: {
          l_.Expect(Token::NUMBER);
          offset = +l_.Int();
          l_.NextToken();
          break;
        }
        case Token::MINUS: {
          l_.Expect(Token::NUMBER);
          offset = -l_.Int();
          l_.NextToken();
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
    default: {
      l_.Error("unexpected token, expected value");
    }
  }
}

// -----------------------------------------------------------------------------
void Parser::ParseComm(Visibility visibility)
{
  l_.Check(Token::IDENT);
  std::string name(l_.String());
  l_.Expect(Token::COMMA);
  l_.Expect(Token::NUMBER);
  int64_t size = l_.Int();
  int64_t align = 1;
  switch (l_.NextToken()) {
    case Token::COMMA: {
      l_.Expect(Token::NUMBER);
      align = l_.Int();
      l_.Expect(Token::NEWLINE);
    }
    case Token::NEWLINE: {
      break;
    }
    default: {
      l_.Error("invalid token, expected comma or newline");
    }
  }

  if ((align & (align - 1)) != 0) {
    l_.Error("Alignment not a power of two.");
  }

  if (func_) EndFunction();
  data_ = prog_->GetOrCreateData(".data");

  Atom *atom = new Atom(ParseName(name));
  atom->SetAlignment(llvm::Align(align));
  atom->AddItem(new Item(Item::Space{ static_cast<unsigned>(size) }));
  atom->SetVisibility(visibility);

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
  std::string op(l_.String());
  assert(op.size() >= 2 && "empty directive");
  l_.NextToken();
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
      if (op == ".byte") return ParseInt8();
      break;
    }
    case 'c': {
      if (op == ".ctor") return ParseXtor(Xtor::Kind::CTOR);
      if (op == ".call") return ParseCall();
      if (op == ".comm") return ParseComm(Visibility::WEAK_DEFAULT);
      break;
    }
    case 'd': {
      if (op == ".double") return ParseDouble();
      if (op == ".dtor") return ParseXtor(Xtor::Kind::DTOR);
      break;
    }
    case 'e': {
      if (op == ".end") return ParseEnd();
      if (op == ".extern") return ParseExtern();
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
      if (op == ".lcomm") return ParseComm(Visibility::WEAK_HIDDEN);
      break;
    }
    case 'n': {
      if (op == ".noinline") return ParseNoInline();
      break;
    }
    case 'p': {
      if (op == ".p2align") return ParseP2Align();
      if (op == ".protected") return ParseProtected();
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

  l_.Error("unknown directive: " + op);
}

// -----------------------------------------------------------------------------
void Parser::ParseSection()
{
  if (func_) EndFunction();

  atom_ = nullptr;
  object_ = nullptr;

  std::string name;
  switch (l_.GetToken()) {
    case Token::STRING:
    case Token::IDENT: {
      name = l_.String();
      break;
    }
    default: {
      l_.Error("expected string or ident");
    }
  }

  switch (l_.NextToken()) {
    case Token::NEWLINE: {
      break;
    }
    case Token::COMMA: {
      l_.Expect(Token::STRING);
      l_.Expect(Token::COMMA);
      l_.Expect(Token::ANNOT);
      switch (l_.NextToken()) {
        case Token::COMMA: {
          l_.Expect(Token::NUMBER);
          l_.Expect(Token::NEWLINE);
          break;
        }
        case Token::NEWLINE: {
          break;
        }
        default: {
          l_.Error("expected comma or newline");
        }
      }
      break;
    }
    default: {
      l_.Error("expected newline or comma");
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
  if (data_ != nullptr || !func_) {
    l_.Error("not in a text segment");
  }
  return func_;
}

// -----------------------------------------------------------------------------
void Parser::ParseAlign()
{
  l_.Check(Token::NUMBER);
  auto v = l_.Int();
  if ((v & (v - 1)) != 0) {
    l_.Error("Alignment not a power of two.");
  }

  if (v > std::numeric_limits<uint8_t>::max()) {
    l_.Error("Alignment out of bounds");
  }

  if (data_) {
    dataAlign_ = v;
  } else {
    if (func_) {
      EndFunction();
    }
    funcAlign_ = v;
  }
  l_.Expect(Token::NEWLINE);
}

// -----------------------------------------------------------------------------
void Parser::ParseP2Align()
{
  l_.Check(Token::NUMBER);
  unsigned v = l_.Int();
  if (v > CHAR_BIT) {
    l_.Error("Alignment out of bounds");
  }
  if (data_) {
    dataAlign_ = 1u << v;
  } else {
    if (func_) {
      EndFunction();
    }
    funcAlign_ = 1u << v;
  }
  l_.Expect(Token::NEWLINE);
}

// -----------------------------------------------------------------------------
void Parser::ParseEnd()
{
  if (func_) {
    EndFunction();
  } else {
    object_ = nullptr;
    atom_ = nullptr;
  }
  l_.Check(Token::NEWLINE);
}

// -----------------------------------------------------------------------------
void Parser::ParseSpace()
{
  l_.Check(Token::NUMBER);
  unsigned length = l_.Int();
  InData();
  Atom *atom = GetAtom();
  switch (l_.NextToken()) {
    case Token::NEWLINE: {
      atom->AddItem(new Item(Item::Space{ length }));
      break;
    }
    case Token::COMMA: {
      l_.Expect(Token::NUMBER);
      int64_t v = l_.Int();
      if (v == 0) {
        atom->AddItem(new Item(Item::Space{ length }));
      } else {
        for (unsigned i = 0; i < length; ++i) {
          atom->AddItem(new Item(static_cast<int8_t>(v)));
        }
      }
      l_.Expect(Token::NEWLINE);
      break;
    }
    default: {
      l_.Error("expected newline or comma");
    }
  }
}

// -----------------------------------------------------------------------------
void Parser::ParseStackObject()
{
  if (!func_) {
    l_.Error("stack_object not in function");
  }

  l_.Check(Token::NUMBER);
  unsigned index = l_.Int();
  l_.Expect(Token::COMMA);
  l_.Expect(Token::NUMBER);
  unsigned size = l_.Int();
  l_.Expect(Token::COMMA);
  l_.Expect(Token::NUMBER);
  unsigned align = l_.Int();
  l_.Expect(Token::NEWLINE);

  GetFunction()->AddStackObject(index, size, llvm::Align(align));
}

// -----------------------------------------------------------------------------
void Parser::ParseCall()
{
  l_.Check(Token::IDENT);
  if (!func_) {
    l_.Error("stack directive not in function");
  }
  GetFunction()->SetCallingConv(ParseCallingConv(l_.String()));
  l_.Expect(Token::NEWLINE);
}

// -----------------------------------------------------------------------------
void Parser::ParseArgs()
{
  auto *func = GetFunction();

  if (l_.GetToken() == Token::IDENT) {
    std::vector<Type> types;
    do {
      l_.Check(Token::IDENT);
      std::string ty(l_.String());
      switch (ty[0]) {
        case 'i': {
          if (ty == "i8") { types.push_back(Type::I8); continue; }
          if (ty == "i16") { types.push_back(Type::I16); continue; }
          if (ty == "i32") { types.push_back(Type::I32); continue; }
          if (ty == "i64") { types.push_back(Type::I64); continue; }
          if (ty == "i128") { types.push_back(Type::I128); continue; }
          break;
        }
        case 'f': {
          if (ty == "f32") { types.push_back(Type::F32); continue; }
          if (ty == "f64") { types.push_back(Type::F64); continue; }
          if (ty == "f80") { types.push_back(Type::F80); continue; }
          break;
        }
        case 'v': {
          if (ty == "v64") { types.push_back(Type::V64); continue; }
          break;
        }
        default: {
          break;
        }
      }
      l_.Error("invalid type");
    } while (l_.NextToken() == Token::COMMA && l_.NextToken() == Token::IDENT);
    func_->SetParameters(types);
  }
  l_.Check(Token::NEWLINE);
}

// -----------------------------------------------------------------------------
void Parser::ParseVararg()
{
  GetFunction()->SetVarArg(true);
  l_.Check(Token::NEWLINE);
}

// -----------------------------------------------------------------------------
void Parser::ParseVisibility()
{
  l_.Check(Token::IDENT);
  auto vis = ParseVisibility(l_.String());
  if (atom_) {
    atom_->SetVisibility(vis);
  } else if (block_) {
    block_->SetVisibility(vis);
  } else {
    if (!func_) {
      l_.Error("invalid visibility directive");
    }
    GetFunction()->SetVisibility(vis);
  }
  l_.Expect(Token::NEWLINE);
}

// -----------------------------------------------------------------------------
void Parser::ParseNoInline()
{
  if (!func_) {
    l_.Error("noinline directive not in function");
  }
  GetFunction()->SetNoInline(true);
  l_.Check(Token::NEWLINE);
}

// -----------------------------------------------------------------------------
void Parser::ParseGlobl()
{
  l_.Check(Token::IDENT);
  std::string name(l_.String());
  globls_.insert(name);
  weak_.erase(name);
  l_.Expect(Token::NEWLINE);
}

// -----------------------------------------------------------------------------
void Parser::ParseHidden()
{
  l_.Check(Token::IDENT);
  std::string name(l_.String());
  hidden_.insert(name);
  l_.Expect(Token::NEWLINE);
}

// -----------------------------------------------------------------------------
void Parser::ParseWeak()
{
  l_.Check(Token::IDENT);
  std::string name(l_.String());
  weak_.insert(name);
  globls_.erase(name);
  l_.Expect(Token::NEWLINE);
}

// -----------------------------------------------------------------------------
void Parser::ParseFile()
{
  l_.Check(Token::STRING);
  l_.Expect(Token::NEWLINE);
}

// -----------------------------------------------------------------------------
void Parser::ParseLocal()
{
  l_.Check(Token::IDENT);
  std::string name(l_.String());
  weak_.erase(name);
  globls_.erase(name);
  l_.Expect(Token::NEWLINE);
}

// -----------------------------------------------------------------------------
void Parser::ParseIdent()
{
  l_.Check(Token::STRING);
  l_.Expect(Token::NEWLINE);
}

// -----------------------------------------------------------------------------
void Parser::ParseSet()
{
  l_.Check(Token::IDENT);
  auto *to = new Extern(l_.String());
  prog_->AddExtern(to);
  l_.Expect(Token::COMMA);
  l_.Expect(Token::IDENT);
  to->SetAlias(prog_->GetGlobalOrExtern(l_.String()));
  l_.Expect(Token::NEWLINE);
}

// -----------------------------------------------------------------------------
void Parser::ParseExtern()
{
  l_.Check(Token::IDENT);
  std::string name(l_.String());
  switch (l_.NextToken()) {
    case Token::NEWLINE: {
      prog_->AddExtern(new Extern(name));
      return;
    }
    case Token::COMMA: {
      l_.Expect(Token::STRING);
      prog_->AddExtern(new Extern(name, l_.String()));
      l_.Expect(Token::NEWLINE);
      return;
    }
    default: {
      l_.Error("unexpected token");
    }
  }
}

// -----------------------------------------------------------------------------
void Parser::ParseXtor(Xtor::Kind kind)
{
  l_.Check(Token::NUMBER);
  int priority = l_.Int();
  l_.Expect(Token::COMMA);
  l_.Expect(Token::IDENT);
  Global *g = prog_->GetGlobalOrExtern(l_.String());
  prog_->AddXtor(new Xtor(priority, g, kind));
  l_.Expect(Token::NEWLINE);
}

// -----------------------------------------------------------------------------
void Parser::ParseAddrsig()
{
  l_.Check(Token::NEWLINE);
}

// -----------------------------------------------------------------------------
void Parser::ParseProtected()
{
  l_.Check(Token::IDENT);
  l_.Expect(Token::NEWLINE);
}

// -----------------------------------------------------------------------------
void Parser::ParseAddrsigSym()
{
  l_.Check(Token::IDENT);
  l_.Expect(Token::NEWLINE);
}

// -----------------------------------------------------------------------------
void Parser::ParseAscii()
{
  l_.Check(Token::STRING);
  InData();
  GetAtom()->AddItem(new Item(l_.String()));
  l_.Expect(Token::NEWLINE);
}

// -----------------------------------------------------------------------------
void Parser::ParseAsciz()
{
  l_.Check(Token::STRING);
  InData();
  Atom *atom = GetAtom();
  atom->AddItem(new Item(l_.String()));
  atom->AddItem(new Item(static_cast<int8_t>(0)));
  l_.Expect(Token::NEWLINE);
}

// -----------------------------------------------------------------------------
void Parser::InData()
{
  if (data_ == nullptr || func_ != nullptr) {
    l_.Error("not in a data segment");
  }
}

// -----------------------------------------------------------------------------
int64_t Parser::Number()
{
  InData();
  int64_t val;
  if (l_.GetToken() == Token::MINUS) {
    l_.Expect(Token::NUMBER);
    val = -l_.Int();
  } else {
    l_.Check(Token::NUMBER);
    val = l_.Int();
  }
  l_.Expect(Token::NEWLINE);
  return val;
}

// -----------------------------------------------------------------------------
void Parser::CreateBlock(const std::string_view name)
{
  block_ = new Block(name);

  if (!func_->empty()) {
    Block *prev = &*func_->rbegin();
    if (auto term = prev->GetTerminator()) {
      for (Use &use : term->operands()) {
        if (!use) {
          use = block_;
        }
      }
    } else {
      prev->AddInst(new JumpInst(block_, {}));
    }
  }
  func_->AddBlock(block_);
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
  l_.Error("invalid token: " + std::string(str));
}

// -----------------------------------------------------------------------------
CallingConv Parser::ParseCallingConv(const std::string_view str)
{
  static std::vector<std::pair<const char *, CallingConv>> kCallingConv
  {
    std::make_pair("c",          CallingConv::C),
    std::make_pair("caml",       CallingConv::CAML),
    std::make_pair("caml_alloc", CallingConv::CAML_ALLOC),
    std::make_pair("caml_gc",    CallingConv::CAML_GC),
    std::make_pair("setjmp",     CallingConv::SETJMP),
  };

  return ParseToken<CallingConv>(kCallingConv, str);
}

// -----------------------------------------------------------------------------
Visibility Parser::ParseVisibility(const std::string_view str)
{
  static std::vector<std::pair<const char *, Visibility>> kVisibility
  {
    std::make_pair("local",          Visibility::LOCAL),
    std::make_pair("global_default", Visibility::GLOBAL_DEFAULT),
    std::make_pair("global_hidden",  Visibility::GLOBAL_HIDDEN),
    std::make_pair("weak_default",   Visibility::WEAK_DEFAULT),
    std::make_pair("weak_hidden",    Visibility::WEAK_HIDDEN),
  };

  return ParseToken<Visibility>(kVisibility, str);
}

// -----------------------------------------------------------------------------
std::string_view Parser::ParseName(std::string_view ident)
{
  return ident.substr(0, ident.find('@'));
}
