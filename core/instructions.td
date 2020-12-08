// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

class Inst {
  // Name of the type implementing the instruction.
  string Type = "";
}

class Operator : Inst {
}

class Unary : Operator {
}

class Binary : Operator {
}

class Overflow : Operator {
}

class Memory : Inst {
}

class Const : Overflow {
}

class Control : Inst {
}

class Terminator : Inst {
}

// -----------------------------------------------------------------------------
// PHI instruction.
// -----------------------------------------------------------------------------

def PHI : Inst
{
}

// -----------------------------------------------------------------------------
// Control flow.
// -----------------------------------------------------------------------------

class CallSite : Terminator {
}

def CALL : CallSite
{
}

def TAIL_CALL : CallSite
{
}

def INVOKE : CallSite
{
}

def RETURN : Terminator
{
}

def JUMP_COND : Terminator
{
}

def JUMP : Terminator
{
}

def SWITCH : Terminator
{
}

def TRAP : Terminator
{
}

def RAISE : Terminator
{
}

def LANDING_PAD : Control
{
}

// -----------------------------------------------------------------------------
// Memory.
// -----------------------------------------------------------------------------

def LD : Memory
{
  let Type = "Load";
}

def ST : Memory
{
  let Type = "Store";
}

// -----------------------------------------------------------------------------
// Varargs.
// -----------------------------------------------------------------------------

def VASTART : Inst
{
  let Type = "VAStart";
}

// -----------------------------------------------------------------------------
// Dynamic stack allocation.
// -----------------------------------------------------------------------------

def ALLOCA : Inst
{

}

// -----------------------------------------------------------------------------
// Constants.
// -----------------------------------------------------------------------------

def ARG : Const
{

}

def FRAME : Const
{

}

def UNDEF : Const
{

}

// -----------------------------------------------------------------------------
// Ternary select.
// -----------------------------------------------------------------------------

def SELECT : Inst
{

}

// -----------------------------------------------------------------------------
// MOV
// -----------------------------------------------------------------------------
def MOV : Operator
{

}

// -----------------------------------------------------------------------------
// Unary instructions
// -----------------------------------------------------------------------------
def ABS : Unary;
def NEG : Unary;
def SQRT : Unary;
def SIN : Unary;
def COS : Unary;
def SEXT : Unary { let Type = "SExt"; }
def ZEXT : Unary { let Type = "ZExt"; }
def FEXT : Unary { let Type = "FExt"; }
def XEXT : Unary { let Type = "XExt"; }
def TRUNC : Unary;
def EXP : Unary;
def EXP2 : Unary;
def LOG : Unary;
def LOG2 : Unary;
def LOG10 : Unary;
def FCEIL : Unary { let Type = "FCeil"; }
def FFLOOR : Unary { let Type = "FFloor"; }
def POPCNT : Unary { let Type = "PopCount"; }
def CLZ : Unary { let Type = "CLZ"; }
def CTZ : Unary { let Type = "CTZ"; }
def BSWAP : Unary { let Type = "BSwap"; }

// -----------------------------------------------------------------------------
// Binary Instructions
// -----------------------------------------------------------------------------
def ADD : Binary;
def AND : Binary;
def CMP : Binary;
def UDIV : Binary { let Type = "UDiv"; }
def UREM : Binary { let Type = "URem"; }
def SDIV : Binary { let Type = "SDiv"; }
def SREM : Binary { let Type = "SRem"; }
def MUL : Binary;
def OR : Binary;
def ROTL : Binary;
def ROTR : Binary;
def SLL : Binary;
def SRA : Binary;
def SRL : Binary;
def SUB : Binary;
def XOR : Binary;
def POW : Binary;
def COPY_SIGN : Binary;

// -----------------------------------------------------------------------------
// Overflow tests
// -----------------------------------------------------------------------------
def ADDUO : Overflow { let Type = "AddUO"; }
def MULUO : Overflow { let Type = "MulUO"; }
def SUBUO : Overflow { let Type = "SubUO"; }
def ADDSO : Overflow { let Type = "AddSO"; }
def MULSO : Overflow { let Type = "MulSO"; }
def SUBSO : Overflow { let Type = "SubSO"; }

// -----------------------------------------------------------------------------
// Generic hardware instructions.
// -----------------------------------------------------------------------------

def SET : Inst
{

}

def SYSCALL : Inst
{

}

def CLONE : Inst
{

}

// -----------------------------------------------------------------------------
// X86 instructions
// -----------------------------------------------------------------------------

def X86_XCHG : Memory
{

}

def X86_CMP_XCHG : Memory
{

}

def X86_MFENCE : Memory
{
  let Type = "X86_MFence";
}

def X86_FNCLEX : Inst
{
  let Type = "X86_FnClEx";
}

def X86_RDTSC : Inst
{
}

def X86_CPUID : Inst
{
  let Type = "X86_CPUID";
}

class X86_FPUControlInst : Inst
{

}

def X86_FNSTCW : X86_FPUControlInst { let Type = "X86_FnStCw"; }
def X86_FNSTSW : X86_FPUControlInst { let Type = "X86_FnStSw"; }
def X86_FNSTENV : X86_FPUControlInst { let Type = "X86_FnStEnv"; }
def X86_FLDCW : X86_FPUControlInst { let Type = "X86_FLdCw"; }
def X86_FLDENV : X86_FPUControlInst { let Type = "X86_FLdEnv"; }
def X86_LDMXCSR : X86_FPUControlInst { let Type = "X86_LdmXCSR"; }
def X86_STMXCSR : X86_FPUControlInst { let Type = "X86_StmXCSR"; }


// -----------------------------------------------------------------------------
// AArch64 instructions
// -----------------------------------------------------------------------------

def AARCH64_LL : Memory
{
  let Type = "AArch64_LL";
}

def AARCH64_SC : Memory
{
  let Type = "AArch64_SC";
}

def AARCH64_DMB : Memory
{
  let Type = "AArch64_DMB";
}

// -----------------------------------------------------------------------------
// RISC-V instructions
// -----------------------------------------------------------------------------

def RISCV_XCHG : Memory
{

}

def RISCV_CMP_XCHG : Memory
{

}

def RISCV_FENCE : Memory
{

}

def RISCV_GP : Inst
{
  let Type = "RISCV_GP";
}

// -----------------------------------------------------------------------------
// Power instructions
// -----------------------------------------------------------------------------

def PPC_LL : Inst
{
  let Type = "PPC_LL";
}

def PPC_SC : Inst
{
  let Type = "PPC_SC";
}

def PPC_SYNC : Inst
{
  let Type = "PPC_Sync";
}

def PPC_ISYNC : Inst
{
  let Type = "PPC_ISync";
}
