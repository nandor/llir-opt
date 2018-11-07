// This file if part of the genm-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include <cassert>
#include <optional>
#include <queue>
#include <sstream>
#include <stack>
#include <string_view>
#include <vector>

#include <llvm/ADT/SCCIterator.h>

#include "core/block.h"
#include "core/context.h"
#include "core/data.h"
#include "core/dominator.h"
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
  ParserError(unsigned row, unsigned col, const std::string_view &message)
  {
    std::ostringstream os;
    os << "[" << row << "," << col << "]: " << message;
    message_ = os.str();
  }

  /// Appends a string to the message.
  ParserError &operator << (const std::string_view &str)
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
Prog *Parser::Parse()
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
              func_ = func_ ? func_ : prog_->AddFunc(*funcName_);
              block_ = new Block(func_, str_);
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

  if (func_) EndFunction();
  return prog_;
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

  // Make sure we have a correct function.
  func_ = func_ ? func_ : prog_->AddFunc(*funcName_);

  // An instruction is composed of an opcode, followed by optional annotations.
  size_t dot = str_.find('.');
  std::string_view view(str_);
  Inst::Kind op = ParseOpcode(view.substr(0, dot));

  std::optional<size_t> size;
  std::optional<Cond> cc;
  std::vector<Type> types;

  // Parse the tokens composing the opcode - size, condition code and types.
  while (dot != std::string::npos) {
    // Split the string at the next dot.
    size_t next = view.find('.', dot + 1);
    size_t length = next == std::string::npos ? next : (next - dot - 1);
    std::string_view token = view.substr(dot + 1, length);
    if (length == 0) {
      throw ParserError(row_, col_, "invalid opcode ") << str_;
    }

    switch (token[0]) {
      case 'e': {
        if (token == "eq") cc = Cond::EQ;
        break;
      }
      case 'l': {
        if (token == "lt") cc = Cond::LT;
        if (token == "le") cc = Cond::LE;
        break;
      }
      case 'g': {
        if (token == "gt") cc = Cond::GT;
        if (token == "ge") cc = Cond::GE;
        break;
      }
      case 'n': {
        if (token == "neq") cc = Cond::NEQ;
        break;
      }
      case 'i': {
        if (token == "i8") types.push_back(Type::I8);
        if (token == "i16") types.push_back(Type::I16);
        if (token == "i32") types.push_back(Type::I32);
        if (token == "i64") types.push_back(Type::I64);
        break;
      }
      case 'f': {
        if (token == "f32") types.push_back(Type::F32);
        if (token == "f64") types.push_back(Type::F64);
        break;
      }
      case 'o': {
        if (token == "olt") cc = Cond::OLT;
        if (token == "ogt") cc = Cond::OGT;
        if (token == "ole") cc = Cond::OLE;
        if (token == "oge") cc = Cond::OGE;
        break;
      }
      case 'u': {
        if (token == "u8") types.push_back(Type::U8);
        if (token == "u16") types.push_back(Type::U16);
        if (token == "u32") types.push_back(Type::U32);
        if (token == "u64") types.push_back(Type::U64);
        if (token == "ult") cc = Cond::ULT;
        if (token == "ugt") cc = Cond::UGT;
        if (token == "ule") cc = Cond::ULE;
        if (token == "uge") cc = Cond::UGE;
        break;
      }
      default: {
        // Parse integers, i.e. size operands.
        uint64_t sz = 0;
        for (size_t i = 0; i < token.size(); ++i) {
          if (!IsDigit(token[i])) {
            throw ParserError(row_, col_, "invalid opcode ") << str_;
          }
          sz = sz * 10 + ToInt(token[i]);
        }
        size = sz;
        break;
      }
    }

    dot = next;
  }

  // Parse all arguments.
  std::vector<Operand> ops;
  do {
    switch (NextToken()) {
      case Token::NEWLINE: {
        if (!ops.empty()) throw ParserError(row_, col_, "expected argument");
        break;
      }
      // $sp, $fp
      case Token::REG: {
        ops.emplace_back(reg_);
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
      case Token::LBRACE: {
        switch (NextToken()) {
          case Token::REG: {
            ops.emplace_back(reg_);
            break;
          }
          case Token::VREG: {
            ops.emplace_back(reinterpret_cast<Inst *>((vreg_ << 1) | 1));
            break;
          }
          default: {
            throw ParserError(row_, col_, "invalid indirection");
          }
        }
        Expect(Token::RBRACE);
        NextToken();
        break;
      }
      // -123
      case Token::MINUS: {
        Expect(Token::NUMBER);
        ops.emplace_back(-int_);
        NextToken();
        break;
      }
      // 123
      case Token::NUMBER: {
        ops.emplace_back(int_);
        NextToken();
        break;
      }
      // _some_name + offset
      case Token::IDENT: {
        if (!str_.empty() && str_[0] == '.') {
          auto it = blocks_.emplace(str_, nullptr);
          if (it.second) {
            // Forward jump - create a placeholder block.
            it.first->second = new Block(func_, str_);
          }
          ops.emplace_back(it.first->second);
          NextToken();
        } else {
          Symbol *sym = ctx_.CreateSymbol(str_);
          switch (NextToken()) {
            case Token::PLUS: {
              Expect(Token::NUMBER);
              ops.emplace_back(ctx_.CreateSymbolOffset(sym, +int_));
              NextToken();
              break;
            }
            case Token::MINUS: {
              Expect(Token::NUMBER);
              ops.emplace_back(ctx_.CreateSymbolOffset(sym, -int_));
              NextToken();
              break;
            }
            default: {
              ops.emplace_back(sym);
              break;
            }
          }
        }
        break;
      }
      default: {
        throw ParserError(row_, col_, "invalid argument");
      }
    }
  } while (tk_ == Token::COMMA);

  // Parse optional annotations.
  while (tk_ == Token::ANNOT) {
    NextToken();
  }

  // Done, must end with newline.
  Check(Token::NEWLINE);

  // Create a block for the instruction.
  if (block_ == nullptr) {
    // An empty start block, if not explicitly defined.
    block_ = new Block(func_, ".LBBentry");
    topo_.push_back(block_);
  } else if (!block_->IsEmpty()) {
    // If the previous instruction is a terminator, start a new block.
    Inst *l = &*block_->rbegin();
    if (l->IsTerminator()) {
      block_ = new Block(func_, ".LBBterm" + std::to_string(topo_.size()));
      topo_.push_back(block_);
    }
  }

  // Add the instruction to the block.
  Inst *i = CreateInst(op, ops, cc, size, types);
  for (unsigned idx = 0, rets = i->GetNumRets(); idx < rets; ++idx) {
    const auto vreg = reinterpret_cast<uint64_t>(ops[idx].GetInst());
    vregs_[i] = vreg >> 1;
  }

  block_->AddInst(i);
}

// -----------------------------------------------------------------------------
void Parser::ParseBSS()
{
  if (func_) EndFunction();
  data_ = prog_->GetBSS();
}

// -----------------------------------------------------------------------------
void Parser::ParseData()
{
  if (func_) EndFunction();
  data_ = prog_->GetData();
}

// -----------------------------------------------------------------------------
void Parser::ParseConst()
{
  if (func_) EndFunction();
  data_ = prog_->GetConst();
}

// -----------------------------------------------------------------------------
void Parser::ParseText()
{
  if (func_) EndFunction();
  data_ = nullptr;
}

// -----------------------------------------------------------------------------
Inst::Kind Parser::ParseOpcode(const std::string_view op)
{
  assert(op.size() > 0 && "empty token");
  switch (op[0]) {
    case 'a': {
      if (op == "addr") return Inst::Kind::ADDR;
      if (op == "arg") return Inst::Kind::ARG;
      if (op == "abs") return Inst::Kind::ABS;
      if (op == "add") return Inst::Kind::ADD;
      if (op == "and") return Inst::Kind::AND;
      break;
    }
    case 'c': {
      if (op == "call") return Inst::Kind::CALL;
      if (op == "cmp") return Inst::Kind::CMP;
      break;
    }
    case 'd': {
      if (op == "div") return Inst::Kind::DIV;
      break;
    }
    case 'i': {
      if (op == "imm") return Inst::Kind::IMM;
      break;
    }
    case 'j': {
      if (op == "jf") return Inst::Kind::JF;
      if (op == "jt") return Inst::Kind::JT;
      if (op == "ji") return Inst::Kind::JI;
      if (op == "jmp") return Inst::Kind::JMP;
      break;
    }
    case 'l': {
      if (op == "ld") return Inst::Kind::LD;
      break;
    }
    case 'm': {
      if (op == "mov") return Inst::Kind::MOV;
      if (op == "mod") return Inst::Kind::MOD;
      if (op == "mul") return Inst::Kind::MUL;
      if (op == "mulh") return Inst::Kind::MULH;
      break;
    }
    case 'n': {
      if (op == "neg") return Inst::Kind::NEG;
      break;
    }
    case 'o': {
      if (op == "or") return Inst::Kind::OR;
      break;
    }
    case 'p': {
      if (op == "pop") return Inst::Kind::POP;
      if (op == "push") return Inst::Kind::PUSH;
      break;
    }
    case 'r': {
      if (op == "ret") return Inst::Kind::RET;
      if (op == "rem") return Inst::Kind::REM;
      if (op == "rotl") return Inst::Kind::ROTL;
      break;
    }
    case 's': {
      if (op == "select") return Inst::Kind::SELECT;
      if (op == "set") return Inst::Kind::SET;
      if (op == "sext") return Inst::Kind::SEXT;
      if (op == "sll") return Inst::Kind::SLL;
      if (op == "sra") return Inst::Kind::SRA;
      if (op == "srl") return Inst::Kind::SRL;
      if (op == "st") return Inst::Kind::ST;
      if (op == "sub") return Inst::Kind::SUB;
      if (op == "switch") return Inst::Kind::SWITCH;
      break;
    }
    case 't': {
      if (op == "tcall") return Inst::Kind::TCALL;
      if (op == "trunc") return Inst::Kind::TRUNC;
      break;
    }
    case 'x': {
      if (op == "xchg") return Inst::Kind::XCHG;
      if (op == "xor") return Inst::Kind::XOR;
      break;
    }
    case 'z': {
      if (op == "zext") return Inst::Kind::ZEXT;
      break;
    }
  }

  throw ParserError(row_, col_, "unknown opcode: ") << op;
}

// -----------------------------------------------------------------------------
Inst *Parser::CreateInst(
    Inst::Kind kind,
    const std::vector<Operand> &ops,
    const std::optional<Cond> &ccs,
    const std::optional<size_t> &size,
    const std::vector<Type> &ts)
{
  auto op = [&ops](int idx) { return ops[idx]; };
  auto bb = [&ops](int idx) { return ops[idx]; };
  auto imm = [&ops](int idx) { return ops[idx].GetInt(); };
  auto cc = [&ccs]() { return *ccs; };
  auto t = [&ts](int idx) { return ts[idx]; };
  auto sz = [&size]() { return *size; };

  switch (kind) {
    // Jumps.
    case Inst::Kind::JT:     return new JumpTrueInst(block_, op(0), bb(1));
    case Inst::Kind::JF:     return new JumpFalseInst(block_, op(0), bb(1));
    case Inst::Kind::JI:     return new JumpIndirectInst(block_, op(0));
    case Inst::Kind::JMP:    return new JumpInst(block_, bb(0));
    // Memory instructions.
    case Inst::Kind::LD:     return new LoadInst(block_, sz(), t(0), op(1));
    case Inst::Kind::ST:     return new StoreInst(block_, sz(), op(0), op(1));
    case Inst::Kind::PUSH:   return new PushInst(block_, t(0), op(0));
    case Inst::Kind::POP:    return new PopInst(block_, t(0));
    case Inst::Kind::XCHG:   return new ExchangeInst(block_, t(0), op(1), op(2));
    // Constant instructions.
    case Inst::Kind::IMM:    return new ImmInst(block_, t(0), imm(1));
    case Inst::Kind::ARG:    return new ArgInst(block_, t(0), imm(1));
    case Inst::Kind::ADDR:   return new AddrInst(block_, t(0), op(1));
    // Unary instructions.
    case Inst::Kind::ABS:    return new AbsInst(block_, t(0), op(1));
    case Inst::Kind::MOV:    return new MovInst(block_, t(0), op(1));
    case Inst::Kind::NEG:    return new NegInst(block_, t(0), op(1));
    case Inst::Kind::SEXT:   return new SignExtendInst(block_, t(0), op(1));
    case Inst::Kind::ZEXT:   return new ZeroExtendInst(block_, t(0), op(1));
    case Inst::Kind::TRUNC:  return new TruncateInst(block_, t(0), op(1));
    // Binary instructions.
    case Inst::Kind::ADD:    return new AddInst(block_, t(0), op(1), op(2));
    case Inst::Kind::AND:    return new AndInst(block_, t(0), op(1), op(2));
    case Inst::Kind::DIV:    return new DivInst(block_, t(0), op(1), op(2));
    case Inst::Kind::MOD:    return new ModInst(block_, t(0), op(1), op(2));
    case Inst::Kind::MUL:    return new MulInst(block_, t(0), op(1), op(2));
    case Inst::Kind::MULH:   return new MulhInst(block_, t(0), op(1), op(2));
    case Inst::Kind::OR:     return new OrInst(block_, t(0), op(1), op(2));
    case Inst::Kind::ROTL:   return new RotlInst(block_, t(0), op(1), op(2));
    case Inst::Kind::SLL:    return new SllInst(block_, t(0), op(1), op(2));
    case Inst::Kind::SRA:    return new SraInst(block_, t(0), op(1), op(2));
    case Inst::Kind::REM:    return new RemInst(block_, t(0), op(1), op(2));
    case Inst::Kind::SRL:    return new SrlInst(block_, t(0), op(1), op(2));
    case Inst::Kind::SUB:    return new SubInst(block_, t(0), op(1), op(2));
    case Inst::Kind::XOR:    return new XorInst(block_, t(0), op(1), op(2));
    // Compare instruction.
    case Inst::Kind::CMP: {
      return new CmpInst(block_, t(0), cc(), op(1), op(2));
    }
    // Select instruction.
    case Inst::Kind::SELECT: {
      return new SelectInst(block_, t(0), op(1), op(2), op(3));
    }
    // Set instruction.
    case Inst::Kind::SET: {
      return new SetInst(block_, op(0), op(1));
    }
    // Instructions with variable arguments.
    case Inst::Kind::SWITCH: {
      return new SwitchInst(block_, op(0), { ops.begin() + 1, ops.end() });
    }
    case Inst::Kind::RET: {
      if (ts.empty()) {
        return new ReturnInst(block_);
      } else {
        return new ReturnInst(block_, t(0), op(0));
      }
    }
    case Inst::Kind::CALL: {
      if (ts.empty()) {
        return new CallInst(block_, op(0), { ops.begin() + 1, ops.end() });
      } else {
        return new CallInst(block_, t(0), op(1), { ops.begin() + 2, ops.end() });
      }
    }
    case Inst::Kind::TCALL: {
      return new TailCallInst(block_, op(0), { ops.begin() + 1, ops.end() });
    }
    case Inst::Kind::PHI: {
      assert(!"not implemented");
    }
  }
}

// -----------------------------------------------------------------------------
Func *Parser::GetFunction()
{
  func_ = func_ ? func_ : prog_->AddFunc(*funcName_);
  return func_;
}

// -----------------------------------------------------------------------------
void Parser::EndFunction()
{
  // Add the blocks to the function, in order.
  for (Block *block : topo_) {
    func_->AddBlock(block);
  }

  // Construct the dominator tree & find dominance frontiers.
  DominatorTree DT(*func_);
  DominanceFrontier DF;
  DF.analyze(DT);

  // Placement of PHI nodes.
  {
    // Find all definitions of all variables.
    llvm::DenseMap<unsigned, std::queue<Inst *>> sites;
    for (Block &block : *func_) {
      for (Inst &inst : block) {
        if (inst.GetNumRets() > 0) {
          sites[vregs_[&inst]].push(&inst);
        }
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
        auto *block = inst->GetParent();
        for (auto &front : DF.calculate(DT, DT[block])) {
          llvm::errs() << front->GetName().data() << "\n";
          assert(!"not implemented");
        }
      }
    }

    // Renaming variables to point to definitions or PHI nodes.
    llvm::DenseMap<unsigned, std::stack<Inst *>> vars;
    std::function<void(Block *block)> rename = [&](Block *block) {
      // Rename variables in this block, capturing definitions on the stack.
      for (Inst &inst : *block) {
        for (unsigned i = 0, nops = inst.GetNumOps(); i < nops; ++i) {
          const auto &op = inst.GetOp(i);
          if (op.IsInst()) {
            const auto vreg = reinterpret_cast<uint64_t>(op.GetInst());
            assert((vreg & 1 ) && "expected a virtual register.");
            auto &stk = vars[vreg >> 1];
            if (stk.empty()) {
              throw ParserError(row_, col_, "undefined virtual register");
            }
            inst.SetOp(i, stk.top());
          }
        }

        if (inst.GetNumRets() > 0) {
          vars[vregs_[&inst]].push(&inst);
        }
      }

      // Handle PHI nodes in successors.
      for (Block *succ : block->successors()) {
        for (Inst &inst : *succ) {
          if (!inst.Is(Inst::Kind::PHI)) {
            break;
          }
          assert(!"not implemented");
        }
      }

      // Recursively rename child nodes.
      for (const auto *child : *DT[block]) {
        rename(child->getBlock());
      }

      // Pop definitions of this block from the stack.
      for (Inst &inst : *block) {
        if (inst.GetNumRets() > 0) {
          vars[vregs_[&inst]].pop();
        }
      }
    };
    rename(DT.getRoot());
  }

  /*
  // Add the blocks in their original order and replace vregs with pointers.
  std::unordered_map<unsigned, Inst *> allVars;

  */
    /*
    std::cerr << block->GetName() << "\n";
    for (auto &inst : *block) {
      for (unsigned i = 0, nops = inst.GetNumOps(); i < nops; ++i) {
        const auto &op = inst.GetOp(i);
        if (op.IsInst()) {
          const auto vreg = reinterpret_cast<uint64_t>(op.GetInst());
          if (vreg & 1) {
            std::cerr << "Replace " << (vreg >> 1) << " " << vregs_[vreg >> 1] << "\n";
            inst.SetOp(i, vregs_[vreg >> 1]);
          }
        }
      }
    }
  }
    */

  func_ = nullptr;
  block_ = nullptr;

  vregs_.clear();
  blocks_.clear();
  topo_.clear();
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
  if (!funcName_) {
    throw ParserError(row_, col_, "stack directive not in function");
  }
  GetFunction()->SetStackSize(int_);
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
  if (data_ != nullptr || !funcName_) {
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
        if (str_ == "sp") { reg_ = Reg::SP; return tk_ = Token::REG; };
        if (str_ == "fp") { reg_ = Reg::FP; return tk_ = Token::REG; };
        throw ParserError(row_, col_, "unknown register");
      } else {
        throw ParserError(row_, col_, "invalid register name");
      }
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
    };
    throw ParserError(row_, col_)
        << ToString(type) << " expected, got " << ToString(tk_);
  }
}
;
