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
#include <sstream>

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
  , nextLabel_(0)
{
  stk_.emplace(nullptr);
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
      case Token::IDENT: {
        std::string name(ParseName(l_.String()));
        if (l_.NextToken() == Token::COLON) {
          auto &s = GetSection();

          llvm::Align align(s.Align ? *s.Align : 1);
          s.Align = std::nullopt;
          if (s.D == nullptr) {
            if (s.F) {
              // Start a new basic block.
              if (auto *g = prog_->GetGlobal(name)) {
                if (auto *ext = ::cast_or_null<Extern>(g)) {
                  CreateBlock(s.F, name);
                } else {
                  l_.Error(s.F, "redefinition of '" + name + "'");
                }
              } else {
                CreateBlock(s.F, name);
              }
            } else {
              // Start a new function.
              s.F = new Func(name);
              s.F->SetAlignment(align);
              prog_->AddFunc(s.F);
            }
          } else {
            // New atom in a data segment.
            s.A = new Atom(name);
            s.A->SetAlignment(align);
            GetOrCreateObject()->AddAtom(s.A);
          }
          l_.Expect(Token::NEWLINE);
        } else {
          if (!name.empty() && name[0] == '.') {
            ParseDirective(name);
          } else {
            ParseInstruction(name, GetFunction(), GetSection().VRegs);
          }
          l_.Check(Token::NEWLINE);
        }
        continue;
      }
      default: {
        l_.Error("unexpected token, expected operation");
      }
    }
  }

  while (!stk_.empty()) {
    End();
    stk_.pop();
  }

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
void Parser::ParseItem(Type ty)
{
  switch (l_.GetToken()) {
    case Token::MINUS: {
      l_.Expect(Token::NUMBER);
      int64_t value = -l_.Int();
      l_.NextToken();
      switch (ty) {
        case Type::I8: {
          return GetOrCreateAtom()->AddItem(Item::CreateInt8(value));
        }
        case Type::I16: {
          return GetOrCreateAtom()->AddItem(Item::CreateInt16(value));
        }
        case Type::I32: {
          return GetOrCreateAtom()->AddItem(Item::CreateInt32(value));
        }
        case Type::I64: case Type::V64: {
          return GetOrCreateAtom()->AddItem(Item::CreateInt64(value));
        }
        case Type::I128:
        case Type::F32:
        case Type::F64:
        case Type::F128:
        case Type::F80: {
          llvm_unreachable("invalid integer type");
        }
      }
      llvm_unreachable("unknown type");
    }
    case Token::NUMBER: {
      int64_t value = l_.Int();
      l_.NextToken();
      switch (ty) {
        case Type::I8: {
          return GetOrCreateAtom()->AddItem(Item::CreateInt8(value));
        }
        case Type::I16: {
          return GetOrCreateAtom()->AddItem(Item::CreateInt16(value));
        }
        case Type::I32:{
          return GetOrCreateAtom()->AddItem(Item::CreateInt32(value));
        }
        case Type::I64: case Type::V64: {
          return GetOrCreateAtom()->AddItem(Item::CreateInt64(value));
        }
        case Type::F64: {
          union U { double f; int64_t i; } u = { .i = value };
          return GetOrCreateAtom()->AddItem(Item::CreateFloat64(u.f));
        }
        case Type::I128:
        case Type::F32:
        case Type::F128:
        case Type::F80: {
          llvm_unreachable("invalid integer type");
        }
      }
      llvm_unreachable("unknown type");
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
      auto *expr = SymbolOffsetExpr::Create(
          prog_->GetGlobalOrExtern(name),
          offset
      );
      switch (ty) {
        case Type::I32:{
          return GetOrCreateAtom()->AddItem(Item::CreateExpr32(expr));
        }
        case Type::I64:
        case Type::V64: {
          return GetOrCreateAtom()->AddItem(Item::CreateExpr64(expr));
        }
        case Type::I8:
        case Type::I16:
        case Type::I128:
        case Type::F32:
        case Type::F64:
        case Type::F128:
        case Type::F80: {
          llvm_unreachable("invalid integer type");
        }
      }
      llvm_unreachable("unknown type");
    }
    default: {
      l_.Error("unexpected token, expected value");
    }
  }
}

// -----------------------------------------------------------------------------
void Parser::ParseComm(Visibility visibility)
{
  End();

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

  Atom *atom = new Atom(ParseName(name));
  atom->SetAlignment(llvm::Align(align));
  atom->AddItem(Item::CreateSpace(static_cast<unsigned>(size)));
  atom->SetVisibility(visibility);

  Object *object = new Object();
  object->AddAtom(atom);

  Data *data = prog_->GetOrCreateData(".data");
  data->AddObject(object);
}

// -----------------------------------------------------------------------------
void Parser::ParseDirective(const std::string_view op)
{
  assert(op.size() >= 2 && "empty directive");
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
      if (op == ".byte") return ParseItem(Type::I8);
      break;
    }
    case 'c': {
      if (op == ".ctor") return ParseXtor(Xtor::Kind::CTOR);
      if (op == ".call") return ParseCall();
      if (op == ".comm") return ParseComm(Visibility::WEAK_DEFAULT);
      break;
    }
    case 'd': {
      if (op == ".double") return ParseItem(Type::F64);
      if (op == ".dtor") return ParseXtor(Xtor::Kind::DTOR);
      break;
    }
    case 'e': {
      if (op == ".end") return ParseEnd();
      if (op == ".extern") return ParseExtern();
      if (op == ".equ") return ParseSet();
      break;
    }
    case 'f': {
      if (op == ".file") return ParseFile();
      if (op == ".features") return ParseFeatures();
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
      if (op == ".long") return ParseItem(Type::I32);
      if (op == ".local") return ParseLocal();
      if (op == ".lcomm") return ParseComm(Visibility::WEAK_HIDDEN);
      break;
    }
    case 'n': {
      if (op == ".noinline") return ParseNoInline();
      break;
    }
    case 'p': {
      if (op == ".pushsection") return ParseSection(true);
      if (op == ".p2align") return ParseP2Align();
      if (op == ".protected") return ParseProtected();
      if (op == ".popsection") return ParsePopSection();
      break;
    }
    case 'q': {
      if (op == ".quad") return ParseItem(Type::I64);
      break;
    }
    case 's': {
      if (op == ".short") return ParseItem(Type::I16);
      if (op == ".space") return ParseSpace();
      if (op == ".stack_object") return ParseStackObject();
      if (op == ".section") return ParseSection(false);
      if (op == ".set") return ParseSet();
      break;
    }
    case 't': {
      if (op == ".thread_local") return ParseThreadLocal();
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

  l_.Error("unknown directive: " + std::string(op));
}

// -----------------------------------------------------------------------------
void Parser::ParseSection(bool push)
{
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

  if (!push && !stk_.empty()) {
    End();
    stk_.pop();
  }

  if (name.substr(0, 5) == ".text") {
    stk_.emplace(nullptr);
  } else {
    stk_.emplace(prog_->GetOrCreateData(name));
  }
}

// -----------------------------------------------------------------------------
void Parser::ParseThreadLocal()
{
  End();
  GetOrCreateObject()->SetThreadLocal();
  l_.Check(Token::NEWLINE);
}

// -----------------------------------------------------------------------------
void Parser::ParsePopSection()
{
  if (stk_.empty()) {
    l_.Error("no section to pop");
  }
  End();
  stk_.pop();
  l_.Check(Token::NEWLINE);
}

// -----------------------------------------------------------------------------
void Parser::ParseAlign()
{
  l_.Check(Token::NUMBER);
  auto v = l_.Int();
  if ((v & (v - 1)) != 0) {
    l_.Error("Alignment not a power of two.");
  }

  if (v > std::numeric_limits<uint32_t>::max()) {
    l_.Error("Alignment out of bounds");
  }

  Align(v);
  l_.Expect(Token::NEWLINE);
}

// -----------------------------------------------------------------------------
void Parser::ParseP2Align()
{
  l_.Check(Token::NUMBER);
  unsigned v = l_.Int();
  if (v > std::numeric_limits<uint32_t>::max()) {
    l_.Error("Alignment out of bounds");
  }
  Align(1u << v);
  l_.Expect(Token::NEWLINE);
}

// -----------------------------------------------------------------------------
void Parser::ParseEnd()
{
  End();
  l_.Check(Token::NEWLINE);
}

// -----------------------------------------------------------------------------
void Parser::ParseSpace()
{
  l_.Check(Token::NUMBER);
  unsigned length = l_.Int();
  Atom *atom = GetOrCreateAtom();
  switch (l_.NextToken()) {
    case Token::NEWLINE: {
      atom->AddItem(Item::CreateSpace(length));
      break;
    }
    case Token::COMMA: {
      l_.Expect(Token::NUMBER);
      int64_t v = l_.Int();
      if (v == 0) {
        atom->AddItem(Item::CreateSpace(length));
      } else {
        for (unsigned i = 0; i < length; ++i) {
          atom->AddItem(Item::CreateInt8(v));
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
void Parser::ParseFeatures()
{
  l_.Check(Token::STRING);
  GetFunction()->SetCPU(l_.String());
  l_.Expect(Token::COMMA);
  l_.Expect(Token::STRING);
  GetFunction()->SetTuneCPU(l_.String());
  l_.Expect(Token::COMMA);
  l_.Expect(Token::STRING);
  GetFunction()->SetFeatures(l_.String());
  l_.Expect(Token::NEWLINE);
}

// -----------------------------------------------------------------------------
void Parser::ParseStackObject()
{
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
  GetFunction()->SetCallingConv(ParseCallingConv(l_.String()));
  l_.Expect(Token::NEWLINE);
}

// -----------------------------------------------------------------------------
void Parser::ParseArgs()
{
  auto *func = GetFunction();

  if (l_.GetToken() == Token::IDENT) {
    std::vector<FlaggedType> params;
    do {
      l_.Check(Token::IDENT);
      Type ty = ParseType(l_.String());
      if (l_.NextToken() == Token::COLON) {
        l_.Expect(Token::IDENT);
        params.emplace_back(ty, ParseTypeFlags(l_.String()));
      } else {
        params.emplace_back(ty);
      }
    } while (l_.GetToken() == Token::COMMA && l_.NextToken() == Token::IDENT);
    func->SetParameters(params);
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
  auto &s = GetSection();
  if (s.A) {
    s.A->SetVisibility(vis);
  } else {
    if (!s.F->empty()) {
      s.F->rbegin()->SetVisibility(vis);
    } else {
      s.F->SetVisibility(vis);
    }
  }
  l_.Expect(Token::NEWLINE);
}

// -----------------------------------------------------------------------------
void Parser::ParseNoInline()
{
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
  switch (l_.NextToken()) {
    default: {
      l_.Error("expected global or constant");
    }
    case Token::IDENT: {
      to->SetValue(prog_->GetGlobalOrExtern(l_.String()));
      l_.Expect(Token::NEWLINE);
      return;
    }
    case Token::MINUS: {
      l_.Expect(Token::NUMBER);
      to->SetValue(new ConstantInt(-l_.Int()));
      l_.Expect(Token::NEWLINE);
      return;
    }
    case Token::NUMBER: {
      to->SetValue(new ConstantInt(l_.Int()));
      l_.Expect(Token::NEWLINE);
      return;
    }
  }
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
  GetOrCreateAtom()->AddItem(Item::CreateString(l_.String()));
  l_.Expect(Token::NEWLINE);
}

// -----------------------------------------------------------------------------
void Parser::ParseAsciz()
{
  l_.Check(Token::STRING);
  Atom *atom = GetOrCreateAtom();
  atom->AddItem(Item::CreateString(l_.String()));
  atom->AddItem(Item::CreateInt8(0));
  l_.Expect(Token::NEWLINE);
}

// -----------------------------------------------------------------------------
Parser::Section &Parser::GetSection()
{
  if (stk_.empty()) {
    l_.Error("missing section");
  }
  return stk_.top();
}

// -----------------------------------------------------------------------------
Object *Parser::GetOrCreateObject()
{
  auto &s = GetSection();
  if (s.O) {
    return s.O;
  }
  s.O = new Object();
  s.D->AddObject(s.O);
  return s.O;
}

// -----------------------------------------------------------------------------
Atom *Parser::GetOrCreateAtom()
{
  auto &s = GetSection();

  if (s.D == nullptr || s.F != nullptr) {
    l_.Error("not in a data segment");
  }

  if (!s.A) {
    if (!s.O) {
      s.O = new Object();
      s.D->AddObject(s.O);
    }
    s.A = new Atom((".L" + s.D->getName() + "$begin").str());
    s.O->AddAtom(s.A);

    if (s.Align) {
      s.A->SetAlignment(llvm::Align(*s.Align));
      s.Align = std::nullopt;
    }
  }

  return s.A;
}

// -----------------------------------------------------------------------------
Func *Parser::GetFunction()
{
  auto &s = GetSection();
  if (s.D != nullptr || !s.F) {
    l_.Error("not in a text segment");
  }
  return s.F;
}

// -----------------------------------------------------------------------------
void Parser::EndFunction()
{
  auto &s = GetSection();

  // Check if function is ill-defined.
  if (s.F->empty()) {
    l_.Error(s.F, "Empty function");
  } else if (!s.F->rbegin()->GetTerminator()) {
    l_.Error(s.F, "Function not terminated");
  }

  if (auto err = PhiPlacement(*s.F, s.VRegs)) {
    llvm::handleAllErrors(std::move(err), [&](const llvm::ErrorInfoBase &e) {
      l_.Error(s.F, e.message());
    });
  }

  s.F = nullptr;
  s.VRegs.clear();
}

// -----------------------------------------------------------------------------
void Parser::Align(int64_t alignment)
{
  auto &s = GetSection();
  if (s.F) {
    EndFunction();
  }
  s.A = nullptr;
  s.Align = alignment;
}

// -----------------------------------------------------------------------------
void Parser::End()
{
  auto &s = GetSection();
  if (s.F) {
    EndFunction();
  }
  s.A = nullptr;
  s.Align = std::nullopt;
  s.O = nullptr;
}

// -----------------------------------------------------------------------------
int64_t Parser::Number()
{
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
void Parser::CreateBlock(Func *func, const std::string_view name)
{
  Block *block = new Block(name);

  if (!func->empty()) {
    Block *prev = &*func->rbegin();
    if (auto term = prev->GetTerminator()) {
      for (Use &use : term->operands()) {
        if (!use) {
          use = block;
        }
      }
    } else {
      prev->AddInst(new JumpInst(block, {}));
    }
  }
  func->AddBlock(block);
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
Type Parser::ParseType(const std::string_view str)
{
  static std::vector<std::pair<const char *, Type>> kTypes
  {
    std::make_pair("i8",           Type::I8),
    std::make_pair("i16",          Type::I16),
    std::make_pair("i32",          Type::I32),
    std::make_pair("i64",          Type::I64),
    std::make_pair("v64",          Type::V64),
    std::make_pair("i128",         Type::I128),
    std::make_pair("f32",          Type::F32),
    std::make_pair("f64",          Type::F64),
    std::make_pair("f80",          Type::F80),
    std::make_pair("f128",         Type::F128),
  };

  return ParseToken<Type>(kTypes, str);
}

// -----------------------------------------------------------------------------
TypeFlag Parser::ParseTypeFlags(const std::string_view flag)
{
  if (flag == "sext") {
    l_.NextToken();
    return TypeFlag::GetSExt();
  } else if (flag == "zext") {
    l_.NextToken();
    return TypeFlag::GetZExt();
  } else if (flag == "byval") {
    l_.Expect(Token::COLON);
    l_.Expect(Token::NUMBER);
    unsigned size = l_.Int();
    l_.Expect(Token::COLON);
    l_.Expect(Token::NUMBER);
    unsigned align = l_.Int();
    l_.NextToken();
    return TypeFlag::GetByVal(size, llvm::Align(align));
  }
  l_.Error("invalid token: '" + std::string(flag) + "'");
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
    std::make_pair("xen",        CallingConv::XEN),
    std::make_pair("intr",       CallingConv::INTR),
    std::make_pair("multiboot",  CallingConv::MULTIBOOT),
    std::make_pair("win64",      CallingConv::WIN64),
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
