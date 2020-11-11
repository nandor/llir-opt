// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

#include "core/cast.h"
#include "core/parser.h"
#include "core/prog.h"
#include "core/block.h"
#include "core/insts.h"



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
        if (token == "f128") { types.push_back(Type::F128); continue; }
        break;
      }
      case 'o': {
        if (token == "o") { cc = Cond::O; continue; }
        if (token == "oeq") { cc = Cond::OEQ; continue; }
        if (token == "one") { cc = Cond::ONE; continue; }
        if (token == "olt") { cc = Cond::OLT; continue; }
        if (token == "ogt") { cc = Cond::OGT; continue; }
        if (token == "ole") { cc = Cond::OLE; continue; }
        if (token == "oge") { cc = Cond::OGE; continue; }
        break;
      }
      case 'u': {
        if (token == "uo") { cc = Cond::UO; continue; }
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
  std::vector<Ref<Value>> ops;
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
      continue;
    }
    l_.Error("invalid annotation");
  }

  // Done, must end with newline.
  l_.Check(Token::NEWLINE);

  // Create a block for the instruction.
  if (block_ == nullptr) {
    // An empty start block, if not explicitly defined.
    CreateBlock(".LBBentry" + std::to_string(++nextLabel_));
  } else if (!block_->empty()) {
    // If the previous instruction is a terminator, start a new block.
    Inst *l = &*block_->rbegin();
    if (l->IsTerminator()) {
      CreateBlock(".LBBterm" + std::to_string(++nextLabel_));
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
    const auto vreg = reinterpret_cast<uint64_t>(ops[idx].Get());
    vregs_[i->GetSubValue(idx)] = vreg >> 1;
  }

  block_->AddInst(i);
}


// -----------------------------------------------------------------------------
Inst *Parser::CreateInst(
    const std::string &opc,
    const std::vector<Ref<Value>> &ops,
    const std::optional<Cond> &ccs,
    const std::optional<size_t> &size,
    const std::vector<Type> &ts,
    const std::optional<CallingConv> &conv,
    bool strict,
    AnnotSet &&annot)
{
  auto val = [&, this] (int idx) {
    if ((idx < 0 && -idx > ops.size()) || (idx >= 0 && idx >= ops.size())) {
      l_.Error("Missing operand");
    }
    return idx >= 0 ? ops[idx] : *(ops.end() + idx);
  };
  auto t = [&, this] (int idx) {
    if ((idx < 0 && -idx > ts.size()) || (idx >= 0 && idx >= ts.size())) {
      l_.Error("Missing type");
    }
    return idx >= 0 ? ts[idx] : *(ts.end() + idx);
  };
  auto op = [&, this] (int idx) -> Ref<Inst> {
    const auto &ref = val(idx);
    if ((reinterpret_cast<uintptr_t>(ref.Get()) & 1) == 0) {
      l_.Error("vreg expected");
    }
    return Ref(static_cast<Inst *>(ref.Get()), ref.Index());
  };
  auto is_sym = [&, this] (int idx) -> bool {
    const auto &ref = val(idx);
    if ((reinterpret_cast<uintptr_t>(ref.Get()) & 1) != 0) {
      return false;
    }
    if (!ref->Is(Value::Kind::GLOBAL)) {
      return false;
    }
    return true;
  };
  auto sym = [&, this] (int idx) -> Block * {
    if (!is_sym(idx)) {
      l_.Error(func_, "not a global");
    }
    return static_cast<Block *>(val(idx).Get());
  };
  auto imm = [&, this](int idx) {
    return ::cast<ConstantInt>(val(idx)).Get();
  };
  auto reg = [&, this](int idx) {
    return ::cast<ConstantReg>(val(idx)).Get();
  };
  auto cc = [&, this] () { return *ccs; };
  auto sz = [&, this] () { return *size; };
  auto call = [&, this] () {
    if (!conv) {
      l_.Error("missing calling conv");
    }
    return *conv;
  };
  auto args = [&, this](int beg, int end) {
    std::vector<Ref<Inst>> args;
    for (int i = beg, n = ops.size(); i < n + end; ++i) {
      args.push_back(op(i));
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
      if (opc == "aarch64_ll")  return new AArch64_LL(t(0), op(1), std::move(annot));
      if (opc == "aarch64_sc")  return new AArch64_SC(t(0), op(1), op(2), std::move(annot));
      if (opc == "aarch64_dmb") return new AArch64_DMB(std::move(annot));

      break;
    }
    case 'b': {
      if (opc == "bswap")  return new BSwapInst(t(0), op(1), std::move(annot));
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
          phi->Add(sym(i), op(i + 1));
        }
        return phi;
      }
      if (opc == "popcnt")  return new PopCountInst(t(0), op(1), std::move(annot));
      break;
    }
    case 'r': {
      if (opc == "raise") return new RaiseInst(conv, op(0), op(1), args(2, 0), std::move(annot));
      if (opc == "rotl")  return new RotlInst(t(0), op(1), op(2), std::move(annot));
      if (opc == "rotr")  return new RotrInst(t(0), op(1), op(2), std::move(annot));
      if (opc == "ret")   return new ReturnInst(args(0, 0), std::move(annot));

      if (opc == "riscv_ll")  return new RISCV_LL(t(0), op(1), std::move(annot));
      if (opc == "riscv_sc")  return new RISCV_SC(t(0), op(1), op(2), std::move(annot));
      if (opc == "riscv_fence") return new RISCV_FENCE(std::move(annot));
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
        for (size_t i = 1, n = ops.size(); i < n; ++i) {
          blocks.push_back(sym(i));
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
      if (opc == "x86_fnstcw")  return new X86_FnStCwInst(op(0), std::move(annot));
      if (opc == "x86_fnstsw")  return new X86_FnStSwInst(op(0), std::move(annot));
      if (opc == "x86_fnstenv") return new X86_FnStEnvInst(op(0), std::move(annot));
      if (opc == "x86_fldcw")   return new X86_FLdCwInst(op(0), std::move(annot));
      if (opc == "x86_fldenv")  return new X86_FLdEnvInst(op(0), std::move(annot));
      if (opc == "x86_ldmxcsr") return new X86_LdmXCSRInst(op(0), std::move(annot));
      if (opc == "x86_stmxcsr") return new X86_StmXCSRInst(op(0), std::move(annot));
      if (opc == "x86_fnclex")  return new X86_FnClExInst(std::move(annot));
      if (opc == "x86_rdtsc")   return new X86_RdtscInst(t(0), std::move(annot));
      break;
    }
    case 'z': {
      if (opc == "zext") return new ZExtInst(t(0), op(1), std::move(annot));
      break;
    }
  }

  l_.Error("unknown opcode: " + opc);
}
