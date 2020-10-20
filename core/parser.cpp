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
static std::string_view ParseName(std::string_view ident)
{
  return ident.substr(0, ident.find('@'));
}

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
              if (auto *ext = ::dyn_cast_or_null<Extern>(g)) {
                block_ = new Block(name);
                func_->AddBlock(block_);
              } else {
                l_.Error(func_, "redefinition of '" + name + "'");
              }
            } else {
              block_ = new Block(name);
              func_->AddBlock(block_);
            }
            topo_.push_back(block_);
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
        if (auto *ext = ::dyn_cast_or_null<Extern>(g)) {
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
void Parser::ParseInstruction()
{
  // Make sure we have a correct function.
  Func *func = GetFunction();

  // An instruction is composed of an opcode, followed by optional annotations.
  std::string opcode(l_.String());
  size_t dot = opcode.find('.');
  std::string op = opcode.substr(0, dot);

  std::optional<size_t> size;
  std::optional<Cond> cc;
  std::vector<Type> types;
  std::optional<CallingConv> conv;
  bool strict = false;

  // Parse the tokens composing the opcode - size, condition code and types.
  while (dot != std::string::npos) {
    // Split the string at the next dot.
    size_t next = opcode.find('.', dot + 1);
    size_t length = next == std::string::npos ? next : (next - dot - 1);
    std::string_view token = std::string_view(opcode).substr(dot + 1, length);
    if (length == 0) {
      l_.Error("invalid opcode " + opcode);
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
      case 'v': {
        if (token == "v64") { types.push_back(Type::V64); continue; }
        break;
      }
      case 's': {
        if (token == "strict") { strict = true; continue; }
        break;
      }
      default: {
        if (isdigit(token[0])) {
          // Parse integers, i.e. size operands.
          uint64_t sz = 0;
          for (size_t i = 0; i < token.size(); ++i) {
            if (!isdigit(token[i])) {
              l_.Error("invalid opcode " + opcode);
            }
            sz = sz * 10 + token[i] - '0';
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
    switch (l_.NextToken()) {
      case Token::NEWLINE: {
        if (!ops.empty()) l_.Error("expected argument");
        break;
      }
      // $sp, $fp
      case Token::REG: {
        ops.emplace_back(new ConstantReg(l_.Reg()));
        l_.NextToken();
        break;
      }
      // $123
      case Token::VREG: {
        ops.emplace_back(reinterpret_cast<Inst *>((l_.VReg() << 1) | 1));
        l_.NextToken();
        break;
      }
      // [$123] or [$sp]
      case Token::LBRACKET: {
        switch (l_.NextToken()) {
          case Token::REG: {
            ops.emplace_back(new ConstantReg(l_.Reg()));
            break;
          }
          case Token::VREG: {
            ops.emplace_back(reinterpret_cast<Inst *>((l_.VReg() << 1) | 1));
            break;
          }
          default: {
            l_.Error("invalid indirection");
          }
        }
        l_.Expect(Token::RBRACKET);
        l_.NextToken();
        break;
      }
      // -123
      case Token::MINUS: {
        l_.Expect(Token::NUMBER);
        ops.emplace_back(new ConstantInt(-l_.Int()));
        l_.NextToken();
        break;
      }
      // 123
      case Token::NUMBER: {
        ops.emplace_back(new ConstantInt(+l_.Int()));
        l_.NextToken();
        break;
      }
      // _some_name + offset
      case Token::IDENT: {
        std::string name(ParseName(l_.String()));
        Global *global = prog_->GetGlobalOrExtern(name);
        switch (l_.NextToken()) {
          case Token::PLUS: {
            l_.Expect(Token::NUMBER);
            ops.emplace_back(new SymbolOffsetExpr(global, +l_.Int()));
            l_.NextToken();
            break;
          }
          case Token::MINUS: {
            l_.Expect(Token::NUMBER);
            ops.emplace_back(new SymbolOffsetExpr(global, -l_.Int()));
            l_.NextToken();
            break;
          }
          default: {
            ops.emplace_back(global);
            break;
          }
        }
        break;
      }
      default: {
        l_.Error("invalid argument");
      }
    }
  } while (l_.GetToken() == Token::COMMA);

  // Parse optional annotations.
  AnnotSet annot;
  while (l_.GetToken() == Token::ANNOT) {
    std::string name(l_.String());
    l_.NextToken();

    if (name == "caml_frame") {
      std::vector<size_t> allocs;
      std::vector<CamlFrame::DebugInfos> infos;

      auto sexp = l_.ParseSExp();
      if (auto *list = sexp.AsList()) {
        switch (list->size()) {
          case 0: break;
          case 2: {
            auto *sallocs = (*list)[0].AsList();
            auto *sinfos = (*list)[1].AsList();
            if (!sallocs || !sinfos) {
              l_.Error("invalid @caml_frame descriptor");
            }

            for (size_t i = 0; i < sallocs->size(); ++i) {
              if (auto *number = (*sallocs)[i].AsNumber()) {
                allocs.push_back(number->Get());
                continue;
              }
              l_.Error("invalid allocation descriptor");
            }

            for (size_t i = 0; i < sinfos->size(); ++i) {
              if (auto *sinfo = (*sinfos)[i].AsList()) {
                CamlFrame::DebugInfos info;
                for (size_t j = 0; j < sinfo->size(); ++j) {
                  if (auto *sdebug = (*sinfo)[j].AsList()) {
                    if (sdebug->size() != 3) {
                      l_.Error("malformed debug info descriptor");
                    }
                    auto *sloc = (*sdebug)[0].AsNumber();
                    auto *sfile = (*sdebug)[1].AsString();
                    auto *sdef = (*sdebug)[2].AsString();
                    if (!sloc || !sfile || !sdef) {
                      l_.Error("missing debug info fields");
                    }

                    CamlFrame::DebugInfo debug;
                    debug.Location = sloc->Get();
                    debug.File = sfile->Get();
                    debug.Definition = sdef->Get();
                    info.push_back(std::move(debug));
                    continue;
                  }
                  l_.Error("invalid debug info descriptor");
                }
                infos.push_back(std::move(info));
                continue;
              }
              l_.Error("invalid debug infos descriptor");
            }
            break;
          }
          default: {
            l_.Error("malformed @caml_frame descriptor");
          }
        }
      }

      if (!annot.Set<CamlFrame>(std::move(allocs), std::move(infos))) {
        l_.Error("duplicate @caml_frame");
      }
      continue;
    }
    l_.Error("invalid annotation");
  }

  // Done, must end with newline.
  l_.Check(Token::NEWLINE);

  // Create a block for the instruction.
  if (block_ == nullptr) {
    // An empty start block, if not explicitly defined.
    block_ = new Block(".LBBentry" + std::to_string(++nextLabel_));
    func->AddBlock(block_);
    topo_.push_back(block_);
  } else if (!block_->empty()) {
    // If the previous instruction is a terminator, start a new block.
    Inst *l = &*block_->rbegin();
    if (l->IsTerminator()) {
      block_ = new Block(".LBBterm" + std::to_string(++nextLabel_));
      func->AddBlock(block_);
      topo_.push_back(block_);
    }
  }

  // Add the instruction to the block.
  Inst *i = CreateInst(
      op,
      ops,
      cc,
      size,
      types,
      conv,
      strict,
      std::move(annot)
  );
  for (unsigned idx = 0, rets = i->GetNumRets(); idx < rets; ++idx) {
    const auto vreg = reinterpret_cast<uint64_t>(ops[idx]);
    vregs_[i] = vreg >> 1;
  }

  block_->AddInst(i);
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
Inst *Parser::CreateInst(
    const std::string &opc,
    const std::vector<Value *> &ops,
    const std::optional<Cond> &ccs,
    const std::optional<size_t> &size,
    const std::vector<Type> &ts,
    const std::optional<CallingConv> &conv,
    bool strict,
    AnnotSet &&annot)
{
  auto val = [this, &ops](int idx) {
    if ((idx < 0 && -idx > ops.size()) || (idx >= 0 && idx >= ops.size())) {
      l_.Error("Missing operand");
    }
    return idx >= 0 ? ops[idx] : *(ops.end() + idx);
  };
  auto t = [this, &ts](int idx) {
    if ((idx < 0 && -idx > ts.size()) || (idx >= 0 && idx >= ts.size())) {
      l_.Error("Missing type");
    }
    return idx >= 0 ? ts[idx] : *(ts.end() + idx);
  };
  auto op = [this, &val](int idx) {
    Value *v = val(idx);
    if ((reinterpret_cast<uintptr_t>(v) & 1) == 0) {
      l_.Error("vreg expected");
    }
    return static_cast<Inst *>(v);
  };
  auto is_sym = [this, &val](int idx) {
    Value *v = val(idx);
    if ((reinterpret_cast<uintptr_t>(v) & 1) != 0 || !v->Is(Value::Kind::GLOBAL)) {
      return false;
    }
    return true;
  };
  auto sym = [this, &val, &is_sym](int idx) {
    if (!is_sym(idx)) {
      l_.Error(func_, "not a global");
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
      l_.Error("missing calling conv");
    }
    return *conv;
  };
  auto args = [this, &ops](int beg, int end) {
    std::vector<Inst *> args;
    for (auto it = ops.begin() + beg; it != ops.end() + end; ++it) {
      if ((reinterpret_cast<uintptr_t>(*it) & 1) == 0) {
        l_.Error("vreg expected");
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
      if (opc == "cos")  return new CosInst(t(0), op(1), std::move(annot));
      if (opc == "copysign") return new CopySignInst(t(0), op(1), op(2), std::move(annot));
      if (opc == "clone") {
        return new CloneInst(
            t(0),
            op(1),
            op(2),
            op(3),
            op(4),
            op(5),
            op(6),
            op(7),
            std::move(annot)
        );
      }
      if (opc == "call") {
        if (is_sym(-1)) {
          return new CallInst(
              ts,
              op(ts.size()),
              args(1 + ts.size(), -1),
              sym(-1),
              size.value_or(ops.size() - 2 - ts.size()),
              call(),
              std::move(annot)
          );
        } else {
          return new CallInst(
              ts,
              op(ts.size()),
              args(1 + ts.size(), 0),
              nullptr,
              size.value_or(ops.size() - 1 - ts.size()),
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
        if (is_sym(-2)) {
          return new InvokeInst(
              ts,
              op(ts.size()),
              args(1 + ts.size(), -2),
              sym(-2),
              sym(-1),
              ops.size() - 3 - ts.size(),
              call(),
              std::move(annot)
          );
        } else {
          return new InvokeInst(
              ts,
              op(ts.size()),
              args(1 + ts.size(), -1),
              nullptr,
              sym(-1),
              ops.size() - 2 - ts.size(),
              call(),
              std::move(annot)
          );
        }
      }
      break;
    }
    case 'f': {
      if (opc == "fext")   return new FExtInst(t(0), op(1), std::move(annot));
      if (opc == "frame")  return new FrameInst(t(0), imm(1), imm(2), std::move(annot));
      if (opc == "fceil") return new FCeilInst(t(0), op(1), std::move(annot));
      if (opc == "ffloor") return new FFloorInst(t(0), op(1), std::move(annot));
      break;
    }
    case 'j': {
      if (opc == "jf")    return new JumpCondInst(op(0), nullptr, sym(1), std::move(annot));
      if (opc == "jt")    return new JumpCondInst(op(0), sym(1), nullptr, std::move(annot));
      if (opc == "jmp")   return new JumpInst(sym(0), std::move(annot));
      if (opc == "jcc")   return new JumpCondInst(op(0), sym(1), sym(2), std::move(annot));
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
          l_.Error("Invalid PHI instruction");
        }
        PhiInst *phi = new PhiInst(t(0), std::move(annot));
        for (unsigned i = 1; i < ops.size(); i += 2) {
          auto op = ops[i + 1];
          if ((reinterpret_cast<uintptr_t>(op) & 1) == 0) {
            l_.Error("vreg expected");
          }
          phi->Add(sym(i), static_cast<Inst *>(op));
        }
        return phi;
      }
      if (opc == "popcnt")  return new PopCountInst(t(0), op(1), std::move(annot));
      break;
    }
    case 'r': {
      if (opc == "raise") return new RaiseInst(op(0), op(1), args(2, 0), std::move(annot));
      if (opc == "rotl")  return new RotlInst(t(0), op(1), op(2), std::move(annot));
      if (opc == "rotr")  return new RotrInst(t(0), op(1), op(2), std::move(annot));
      if (opc == "ret") return new ReturnInst(args(0, 0), std::move(annot));
      break;
    }
    case 's': {
      if (opc == "syscall") {
        if (ts.empty()) {
          return new SyscallInst(op(0), args(1, 0), std::move(annot));
        } else {
          return new SyscallInst(t(0), op(1), args(2, 0), std::move(annot));
        }
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
        return new TailCallInst(
            ts,
            op(0),
            args(1, 0),
            size.value_or(ops.size() - 1),
            call(),
            std::move(annot)
        );
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
      if (opc == "xext") return new XExtInst(t(0), op(1), std::move(annot));
      if (opc == "xor")  return new XorInst(t(0), op(1), op(2), std::move(annot));

      if (opc == "x86_xchg")    return new X86_XchgInst(t(0), op(1), op(2), std::move(annot));
      if (opc == "x86_cmpxchg") return new X86_CmpXchgInst(t(0), op(1), op(2), op(3), std::move(annot));
      if (opc == "x86_rdtsc")   return new X86_RdtscInst(t(0), std::move(annot));
      if (opc == "x86_fnstcw")  return new X86_FnStCwInst(op(0), std::move(annot));
      if (opc == "x86_fnstsw")  return new X86_FnStSwInst(op(0), std::move(annot));
      if (opc == "x86_fnstenv") return new X86_FnStEnvInst(op(0), std::move(annot));
      if (opc == "x86_fldcw")   return new X86_FLdCwInst(op(0), std::move(annot));
      if (opc == "x86_fldenv")  return new X86_FLdEnvInst(op(0), std::move(annot));
      if (opc == "x86_ldmxcsr") return new X86_LdmXCSRInst(op(0), std::move(annot));
      if (opc == "x86_stmxcsr") return new X86_StmXCSRInst(op(0), std::move(annot));
      if (opc == "x86_fnclex")  return new X86_FnClExInst(std::move(annot));

      break;
    }
    case 'z': {
      if (opc == "zext") return new ZExtInst(t(0), op(1), std::move(annot));
      break;
    }
  }

  l_.Error("unknown opcode: " + opc);
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
            l_.Error(func_, "Jump falls through");
          } else {
            use = *(it + 1);
          }
        }
      }
    } else if (it + 1 != topo_.end()) {
      block->AddInst(new JumpInst(*(it + 1), {}));
    } else {
      l_.Error(func_, "Unterminated function");
    }
  }

  // Check if function is ill-defined.
  if (func_->empty()) {
    l_.Error(func_, "Empty function");
  }

  PhiPlacement();

  func_ = nullptr;
  block_ = nullptr;

  vregs_.clear();
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
            l_.Error(
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
        if (phi.HasValue(block)) {
          auto *value = phi.GetValue(block);
          const auto vreg = reinterpret_cast<uint64_t>(value);
          if (vreg & 1) {
            phi.Add(block, vars[vreg >> 1].top());
          }
        } else {
          auto &stk = vars[vregs_[&phi]];
          if (!stk.empty()) {
            phi.Add(block, stk.top());
          } else {
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
  std::set<PhiInst *> inQueue;
  for (auto it = func_->begin(); it != func_->end(); ) {
    Block *block = &*it++;
    if (blocks.count(block) == 0) {
      block->replaceAllUsesWith(new ConstantInt(0));
      block->eraseFromParent();
    } else {
      for (auto &phi : block->phis()) {
        if (inQueue.insert(&phi).second) {
          queue.push_back(&phi);
        }
      }
    }
  }

  // Fix up annotations for PHIs: decide between address and value.
  while (!queue.empty()) {
    PhiInst *phi = queue.back();
    queue.pop_back();
    inQueue.erase(phi);

    bool isValue = false;
    for (unsigned i = 0, n = phi->GetNumIncoming(); i < n; ++i) {
      if (auto *inst = ::dyn_cast_or_null<Inst>(phi->GetValue(i))) {
        isValue = isValue || inst->GetType(0) == Type::V64;
      }
    }

    if (!isValue || phi->GetType() == Type::V64) {
      continue;
    }

    PhiInst *newPhi = new PhiInst(Type::V64, phi->GetAnnots());
    for (unsigned i = 0, n = phi->GetNumIncoming(); i < n; ++i) {
      newPhi->Add(phi->GetBlock(i), phi->GetValue(i));
    }
    phi->getParent()->AddInst(newPhi, phi);
    phi->replaceAllUsesWith(newPhi);
    phi->eraseFromParent();

    for (auto *user : newPhi->users()) {
      if (auto *phiUser = ::dyn_cast_or_null<PhiInst>(user)) {
        if (inQueue.insert(phiUser).second) {
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
  } else {
    if (!func_) {
      l_.Error("stack directive not in function");
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
    std::make_pair("caml_raise", CallingConv::CAML_RAISE),
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
