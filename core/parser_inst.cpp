// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/parser.h"

#include "core/cast.h"
#include "core/prog.h"
#include "core/block.h"
#include "core/insts.h"



// -----------------------------------------------------------------------------
void Parser::ParseInstruction(
    const std::string_view opcode,
    Func *func,
    VRegMap &vregs)
{
  // An instruction is composed of an opcode, followed by optional annotations.
  size_t dot = opcode.find('.');
  std::string OpInst(opcode.substr(0, dot));

  std::optional<size_t> size;
  std::optional<Cond> OpCond;
  std::vector<Type> types;
  std::optional<CallingConv> conv;
  bool strict = false;

  // Parse the tokens composing the opcode - size, condition code and types.
  while (dot != std::string::npos) {
    // Split the string at the next dot.
    size_t next = opcode.find('.', dot + 1);
    size_t length = next == std::string::npos ? next : (next - dot - 1);
    std::string_view token = opcode.substr(dot + 1, length);
    if (length == 0) {
      l_.Error("invalid opcode " + std::string(opcode));
    }
    dot = next;

    switch (token[0]) {
      case 'e': {
        if (token == "eq") { OpCond = Cond::EQ; continue; }
        break;
      }
      case 'l': {
        if (token == "lt") { OpCond = Cond::LT; continue; }
        if (token == "le") { OpCond = Cond::LE; continue; }
        break;
      }
      case 'g': {
        if (token == "gt") { OpCond = Cond::GT; continue; }
        if (token == "ge") { OpCond = Cond::GE; continue; }
        break;
      }
      case 'n': {
        if (token == "ne") { OpCond = Cond::NE; continue; }
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
        if (token == "f128") { types.push_back(Type::F128); continue; }
        break;
      }
      case 'o': {
        if (token == "o") { OpCond = Cond::O; continue; }
        if (token == "oeq") { OpCond = Cond::OEQ; continue; }
        if (token == "one") { OpCond = Cond::ONE; continue; }
        if (token == "olt") { OpCond = Cond::OLT; continue; }
        if (token == "ogt") { OpCond = Cond::OGT; continue; }
        if (token == "ole") { OpCond = Cond::OLE; continue; }
        if (token == "oge") { OpCond = Cond::OGE; continue; }
        break;
      }
      case 'u': {
        if (token == "uo") { OpCond = Cond::UO; continue; }
        if (token == "ueq") { OpCond = Cond::UEQ; continue; }
        if (token == "une") { OpCond = Cond::UNE; continue; }
        if (token == "ult") { OpCond = Cond::ULT; continue; }
        if (token == "ugt") { OpCond = Cond::UGT; continue; }
        if (token == "ule") { OpCond = Cond::ULE; continue; }
        if (token == "uge") { OpCond = Cond::UGE; continue; }
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
              l_.Error("invalid opcode " + std::string(opcode));
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
  std::vector<Operand> ops;
  std::vector<std::pair<unsigned, TypeFlag>> flags;
  while (true) {
    switch (l_.GetToken()) {
      case Token::NEWLINE: {
        if (!ops.empty()) l_.Error("expected argument");
        break;
      }
      // $sp, $fp
      case Token::REG: {
        ops.emplace_back(l_.Reg());
        l_.NextToken();
        break;
      }
      // $123
      case Token::VREG: {
        ops.emplace_back(reinterpret_cast<Inst *>((l_.VReg() << 1) | 1));
        if (l_.NextToken() == Token::COLON) {
          l_.Expect(Token::IDENT);
          flags.emplace_back(ops.size() - 1, ParseTypeFlags(l_.String()));
        }
        break;
      }
      // [$123]
      case Token::LBRACKET: {
        l_.Expect(Token::VREG);
        ops.emplace_back(reinterpret_cast<Inst *>((l_.VReg() << 1) | 1));
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
            ops.emplace_back(SymbolOffsetExpr::Create(global, +l_.Int()));
            l_.NextToken();
            break;
          }
          case Token::MINUS: {
            l_.Expect(Token::NUMBER);
            ops.emplace_back(SymbolOffsetExpr::Create(global, -l_.Int()));
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
    if (l_.GetToken() == Token::COMMA) {
      l_.NextToken();
      continue;
    }
    break;
  }

  // Parse optional annotations.
  AnnotSet annot;
  while (l_.GetToken() == Token::ANNOT) {
    std::string name(l_.String());
    l_.NextToken();
    ParseAnnotation(name, annot);
  }

  // Done, must end with newline.
  l_.Check(Token::NEWLINE);

  // Create a block for the instruction.
  if (func->empty()) {
    // An empty start block, if not explicitly defined.
    CreateBlock(func, ".LBBentry" + std::to_string(++nextLabel_));
  } else if (!func->rbegin()->empty()) {
    // If the previous instruction is a terminator, start a new block.
    Inst *l = &*func->rbegin()->rbegin();
    if (l->IsTerminator()) {
      CreateBlock(func, ".LBBterm" + std::to_string(++nextLabel_));
    }
  }

  // Add the instruction to the block.
  Block *block = &*func->rbegin();

  Inst *i = CreateInst(
      func,
      block,
      OpInst,
      ops,
      flags,
      OpCond,
      size,
      types,
      conv,
      strict,
      std::move(annot)
  );

  for (unsigned idx = 0, rets = i->GetNumRets(); idx < rets; ++idx) {
    if (auto vreg = ops[idx].ToVReg()) {
      vregs[i->GetSubValue(idx)] = *vreg >> 1;
    } else {
      l_.Error(func, block, "vreg expected");
    }
  }

  block->AddInst(i);
}


// -----------------------------------------------------------------------------
Inst *Parser::CreateInst(
    Func *func,
    Block *block,
    const std::string &opc,
    const std::vector<Operand> &ops,
    const std::vector<std::pair<unsigned, TypeFlag>> &fs,
    const std::optional<Cond> &ccs,
    const std::optional<size_t> &size,
    const std::vector<Type> &ts,
    const std::optional<CallingConv> &conv,
    bool strict,
    AnnotSet &&annot)
{
  auto arg = [&, this] (int idx) -> const Operand & {
    if ((idx < 0 && -idx > ops.size()) || (idx >= 0 && idx >= ops.size())) {
      l_.Error(func, "Missing operand");
    }
    return idx >= 0 ? ops[idx] : *(ops.end() + idx);
  };
  auto OpValue = [&, this] (int idx) -> Ref<Value> {
    if (auto vreg = arg(idx).ToVal()) {
      return *vreg;
    }
    l_.Error(func, "value expected");
  };
  auto t = [&, this] (int idx) {
    if ((idx < 0 && -idx > ts.size()) || (idx >= 0 && idx >= ts.size())) {
      l_.Error(func, "Missing type");
    }
    return idx >= 0 ? ts[idx] : *(ts.end() + idx);
  };
  auto OpInst = [&, this] (int idx) -> Ref<Inst> {
    const auto &ref = OpValue(idx);
    if ((reinterpret_cast<uintptr_t>(ref.Get()) & 1) == 0) {
      l_.Error(func, block, "vreg expected at '" + opc + "'");
    }
    return Ref(static_cast<Inst *>(ref.Get()), ref.Index());
  };
  auto is_sym = [&, this] (int idx) -> bool {
    const auto &ref = OpValue(idx);
    if ((reinterpret_cast<uintptr_t>(ref.Get()) & 1) != 0) {
      return false;
    }
    if (!ref->Is(Value::Kind::GLOBAL)) {
      return false;
    }
    return true;
  };
  auto OpBlock = [&, this] (int idx) -> Block * {
    if (!is_sym(idx)) {
      l_.Error(func, "not a global");
    }
    return static_cast<Block *>(OpValue(idx).Get());
  };
  auto Opunsigned = [&, this](int idx) {
    return ::cast<ConstantInt>(OpValue(idx)).Get()->GetInt();
  };
  auto Opint = [&, this](int idx) {
    return ::cast<ConstantInt>(OpValue(idx)).Get()->GetInt();
  };
  auto OpRegister = [&, this](int idx) -> Register {
    if (auto r = arg(idx).ToReg()) {
      return *r;
    }
    l_.Error(func, "not a register");
  };
  auto OpCond = [&, this] (int idx) {
    if (!ccs) {
      l_.Error(func, "missing condition code");
    }
    return *ccs;
  };
  auto call = [&, this] () {
    if (!conv) {
      l_.Error(func, "missing calling conv");
    }
    return *conv;
  };
  auto args = [&, this](int beg, int end) {
    std::vector<Ref<Inst>> args;
    for (int i = beg, n = ops.size(); i < n + end; ++i) {
      args.push_back(OpInst(i));
    }
    return args;
  };
  auto flags = [&, this](int beg, int end) {
    std::vector<TypeFlag> flags;
    for (int i = beg, n = ops.size(); i < n + end; ++i) {
      bool found = false;
      for (unsigned j = 0; j < fs.size(); ++j) {
        if (fs[j].first == i) {
          flags.push_back(fs[j].second);
          found = true;
          break;
        }
      }
      if (!found) {
        flags.emplace_back(TypeFlag::GetNone());
      }
    }
    return flags;
  };

  assert(opc.size() > 0 && "empty token");
  switch (opc[0]) {
    case 'c': {
      if (opc == "call") {
        if (is_sym(-1)) {
          return new CallInst(
              ts,
              OpInst(ts.size()),
              args(1 + ts.size(), -1),
              flags(1 + ts.size(), -1),
              call(),
              size,
              OpBlock(-1),
              std::move(annot)
          );
        } else {
          return new CallInst(
              ts,
              OpInst(ts.size()),
              args(1 + ts.size(), 0),
              flags(1 + ts.size(), 0),
              call(),
              size,
              nullptr,
              std::move(annot)
          );
        }
      }
      break;
    }
    case 'f': {
      if (opc == "frame_call") {
        if (is_sym(-1)) {
          return new FrameCallInst(
              ts,
              OpInst(ts.size() + 1),
              args(2 + ts.size(), -1),
              flags(2 + ts.size(), -1),
              call(),
              size,
              OpInst(ts.size()),
              OpBlock(-1),
              std::move(annot)
          );
        } else {
          return new FrameCallInst(
              ts,
              OpInst(ts.size() + 1),
              args(2 + ts.size(), 0),
              flags(2 + ts.size(), 0),
              call(),
              size,
              OpInst(ts.size()),
              nullptr,
              std::move(annot)
          );
        }
      }
      break;
    }
    case 'i': {
      if (opc == "invoke") {
        if (is_sym(-2)) {
          return new InvokeInst(
              ts,
              OpInst(ts.size()),
              args(1 + ts.size(), -2),
              flags(1 + ts.size(), -2),
              call(),
              size,
              OpBlock(-2),
              OpBlock(-1),
              std::move(annot)
          );
        } else {
          return new InvokeInst(
              ts,
              OpInst(ts.size()),
              args(1 + ts.size(), -1),
              flags(1 + ts.size(), -1),
              call(),
              size,
              nullptr,
              OpBlock(-1),
              std::move(annot)
          );
        }
      }
      break;
    }
    case 'j': {
      if (opc == "jf")    return new JumpCondInst(OpInst(0), nullptr, OpBlock(1), std::move(annot));
      if (opc == "jt")    return new JumpCondInst(OpInst(0), OpBlock(1), nullptr, std::move(annot));
      break;
    }
    case 'l': {
      if (opc == "landing_pad") return new LandingPadInst(ts, conv, std::move(annot));
      break;
    }
    case 'p': {
      if (opc == "phi") {
        if ((ops.size() & 1) == 0) {
          l_.Error("Invalid PHI instruction");
        }
        PhiInst *phi = new PhiInst(t(0), std::move(annot));
        for (unsigned i = 1; i < ops.size(); i += 2) {
          phi->Add(OpBlock(i), OpInst(i + 1));
        }
        return phi;
      }
      break;
    }
    case 'r': {
      if (opc == "raise") return new RaiseInst(conv, OpInst(0), OpInst(1), args(2, 0), std::move(annot));
      if (opc == "ret")   return new ReturnInst(args(0, 0), std::move(annot));
      break;
    }
    case 's': {
      if (opc == "syscall") {
        return new SyscallInst(ts, OpInst(ts.size()), args(ts.size() + 1, 0), std::move(annot));
      }
      if (opc == "switch") {
        std::vector<Block *> blocks;
        for (size_t i = 1, n = ops.size(); i < n; ++i) {
          blocks.push_back(OpBlock(i));
        }
        return new SwitchInst(OpInst(0), blocks, std::move(annot));
      }
      break;
    }
    case 't': {
      if (opc == "tcall") {
        return new TailCallInst(
            ts,
            OpInst(0),
            args(1, 0),
            flags(1, 0),
            call(),
            size,
            std::move(annot)
        );
      }
      break;
    }
    case 'x': {
      if (opc == "x86_cpuid") {
        if (ops.size() > 5) {
          return new X86_CpuIdInst(t(0), t(1), t(2), t(3), OpInst(4), OpInst(5), std::move(annot));
        } else {
          return new X86_CpuIdInst(t(0), t(1), t(2), t(3), OpInst(4), std::move(annot));
        }
      }
      break;
    }
  }

  #define GET_PARSER
  #include "instructions.def"

  l_.Error("unknown opcode: " + opc);
}

// -----------------------------------------------------------------------------
void Parser::ParseAnnotation(const std::string_view name, AnnotSet &annot)
{
  if (name == "probability") {
    std::vector<size_t> allocs;
    std::vector<CamlFrame::DebugInfos> infos;

    auto sexp = l_.ParseSExp();
    if (auto *list = sexp.AsList(); list && list->size() == 2) {
      auto *n = (*list)[0].AsNumber();
      auto *d = (*list)[1].AsNumber();
      if (!n || !d) {
        l_.Error("invalid numerator or denumerator");
      }
      if (!annot.Set<Probability>(n->Get(), d->Get())) {
        l_.Error("duplicate @probability");
      }
    }
    return;
  }
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
    return;
  }
  if (name == "cxx_lsda") {
    auto sexp = l_.ParseSExp();
    if (auto *list = sexp.AsList(); list && list->size() == 4) {
      bool isCleanup;
      if (auto cleanup = (*list)[0].AsNumber()) {
        isCleanup = cleanup->Get();
      } else {
        l_.Error("@cxx_lsda expects cleanup flag");
      }

      bool isCatchAll;
      if (auto catchAll = (*list)[1].AsNumber()) {
        isCatchAll = catchAll->Get();
      } else {
        l_.Error("@cxx_lsda expects catch-all flag");
      }

      std::vector<std::string> cs;
      if (auto catchTys = (*list)[2].AsList()) {
        for (unsigned i = 0, n = catchTys->size(); i < n; ++i) {
          if (auto str = (*catchTys)[i].AsString()) {
            cs.push_back(str->Get());
          } else {
            l_.Error("@cxx_lsda expects catch type names");
          }
        }
      } else {
        l_.Error("@cxx_lsda expects catch types");
      }

      std::vector<std::string> fs;
      if (auto filterTys = (*list)[3].AsList()) {
        for (unsigned i = 0, n = filterTys->size(); i < n; ++i) {
          if (auto str = (*filterTys)[i].AsString()) {
            fs.push_back(str->Get());
          } else {
            l_.Error("@cxx_lsda expects filter type names");
          }
        }
      } else {
        l_.Error("@cxx_lsda expects filter types");
      }

      if (!annot.Set<CxxLSDA>(isCleanup, isCatchAll, std::move(cs), std::move(fs))) {
        l_.Error("duplicate @cxx_lsda");
      }
      return;
    } else {
      l_.Error("malformed @cxx_lsda, expecte 4-element tuple");
    }
  }
  l_.Error("invalid annotation");
}
