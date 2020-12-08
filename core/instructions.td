// This file if part of the llir-opt project.
// Licensing information can be found in the LICENSE file.
// (C) 2018 Nandor Licker. All rights reserved.

/**
 * Descriptor for an instruction argument.
 */
class Field {
  string Name;
  string Type;
  bit IsOptional = 0;
  bit IsScalar = 0;
  bit IsList = 0;
}

class InstField<string name> : Field {
  let Name = name;
  let Type = "Inst";
}

class InstListField<string name> : Field {
  let Name = name;
  let IsList = 1;
  let Type = "Inst";
}

class ValueField<string name> : Field {
  let Name = name;
  let Type = "Value";
}

class BlockField<string name> : Field {
  let Name = name;
  let Type = "Block";
}

class BlockListField<string name> : Field {
  let Name = name;
  let IsList = 1;
  let Type = "Block";
}

class UnsignedField<string name> : Field {
  let Name = name;
  let IsScalar = 1;
  let Type = "unsigned";
}

class OptUnsignedField<string name> : Field {
  let Name = name;
  let IsScalar = 1;
  let IsOptional = 1;
  let Type = "unsigned";
}

class RegField<string name> : Field {
  let Name = name;
  let IsScalar = 1;
  let Type = "Register";
}

class CondCodeField<string name> : Field {
  let Name = name;
  let IsScalar = 1;
  let Type = "Cond";
}

class CallingConvField<string name> : Field {
  let Name = name;
  let IsScalar = 1;
  let Type = "CallingConv";
}

class OptCallinvConvField<string name> : Field {
  let Name = name;
  let IsScalar = 1;
  let IsOptional = 1;
  let Type = "CallingConv";
}

/**
 * Base class for the instruction hierarchy.
 */
class Inst {
  // Name of the type implementing the instruction.
  string Type = "";
  // Enumeration of fields.
  list<Field> Fields = [];
  // Custom clone logic required.
  bit HasCustomClone = 0;
  // Custom parser required.
  bit HasCustomParser = 0;
  // Custom reader required.
  bit HasCustomReader = 0;
  // Custom writer required.
  bit HasCustomWriter = 0;
  // Custom printer required.
  bit HasCustomPrinter = 0;
  // Custom definition required.
  bit HasCustomDefinition = 0;
  // Number of values returned.
  int NumTypes = 0;
}

/**
 * Base class for instructions which produce a single value.
 */
class Operator : Inst {
  let NumTypes = 1;
}

/**
 * Base class for unary instructions.
 */
class Unary : Operator {
  let Fields = [ InstField<"Arg"> ];
}

/**
 * Base class for binary instructions.
 */
class Binary : Operator {
  let Fields = [ InstField<"LHS">, InstField<"RHS"> ];
}

class Memory : Inst {
}

class Const : Operator {
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
  bit HasCustomClone = 1;
  bit HasCustomReader = 1;
  bit HasCustomWriter = 1;
}

// -----------------------------------------------------------------------------
// Control flow.
// -----------------------------------------------------------------------------

class CallSite : Terminator {
}

def CALL : CallSite
{
  let Fields = [
    InstField<"Callee">,
    InstListField<"Args">,
    BlockField<"Cont">,
    OptUnsignedField<"NumFixedArgs">,
    CallingConvField<"CallingConv">
  ];
  let NumTypes = -1;
}

def TAIL_CALL : CallSite
{
  let Fields = [
    InstField<"Callee">,
    InstListField<"Args">,
    OptUnsignedField<"NumFixedArgs">,
    CallingConvField<"CallingConv">
  ];
  let NumTypes = -1;
}

def INVOKE : CallSite
{
  let Fields = [
    InstField<"Callee">,
    InstListField<"Args">,
    BlockField<"Cont">,
    BlockField<"Throw">,
    OptUnsignedField<"NumFixedArgs">,
    CallingConvField<"CallingConv">
  ];
  let NumTypes = -1;
}

def RETURN : Terminator
{
  let Fields = [
    InstListField<"Args">
  ];
}

def JUMP_COND : Terminator
{
  let Fields = [
    InstField<"Cond">,
    BlockField<"TrueTarget">,
    BlockField<"FalseTarget">
  ];
}

def JUMP : Terminator
{
  let Fields = [ BlockField<"Target"> ];
}

def SWITCH : Terminator
{
  let Fields = [
    InstField<"Idx">,
    BlockListField<"Blocks">
  ];
}

def TRAP : Terminator
{
}

def RAISE : Terminator
{
  let Fields = [
    OptCallinvConvField<"CallingConv">,
    InstField<"Target">,
    InstField<"Stack">,
    InstListField<"Args">
  ];
}

def LANDING_PAD : Control
{
  let Fields = [ OptCallinvConvField<"CallingConv"> ];
  let NumTypes = -1;
}

// -----------------------------------------------------------------------------
// Memory.
// -----------------------------------------------------------------------------

def LD : Memory
{
  let Type = "Load";
  let Fields = [ InstField<"Addr"> ];
  let NumTypes = 1;
}

def ST : Memory
{
  let Type = "Store";
  let Fields = [ InstField<"Addr">, InstField<"Val"> ];
}

// -----------------------------------------------------------------------------
// Varargs.
// -----------------------------------------------------------------------------

def VASTART : Inst
{
  let Type = "VAStart";
  let Fields = [ InstField<"VAList"> ];
}

// -----------------------------------------------------------------------------
// Dynamic stack allocation.
// -----------------------------------------------------------------------------

def ALLOCA : Inst
{
  let Fields = [
    InstField<"Count">,
    UnsignedField<"Align">
  ];
  let NumTypes = 1;
}

// -----------------------------------------------------------------------------
// Constants.
// -----------------------------------------------------------------------------

def ARG : Const
{
  let Fields = [UnsignedField<"Idx">];
  let NumTypes = 1;
}

def FRAME : Const
{
  let Fields = [
    UnsignedField<"Object">,
    UnsignedField<"Offset">
  ];
  let NumTypes = 1;
}

def UNDEF : Const
{
  let NumTypes = 1;
}

// -----------------------------------------------------------------------------
// Ternary select.
// -----------------------------------------------------------------------------

def SELECT : Inst
{
  let Fields = [ InstField<"Cond">, InstField<"True">, InstField<"False"> ];
  let NumTypes = 1;
}

// -----------------------------------------------------------------------------
// MOV
// -----------------------------------------------------------------------------
def MOV : Operator
{
  let Fields = [ ValueField<"Arg"> ];
  let NumTypes = 1;
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

def CMP : Inst {
  let Fields = [
    CondCodeField<"CC">,
    InstField<"LHS">,
    InstField<"RHS">
  ];
  let NumTypes = 1;
}

// -----------------------------------------------------------------------------
// Overflow tests
// -----------------------------------------------------------------------------
def ADDUO : Binary { let Type = "AddUO"; }
def MULUO : Binary { let Type = "MulUO"; }
def SUBUO : Binary { let Type = "SubUO"; }
def ADDSO : Binary { let Type = "AddSO"; }
def MULSO : Binary { let Type = "MulSO"; }
def SUBSO : Binary { let Type = "SubSO"; }

// -----------------------------------------------------------------------------
// Generic hardware instructions.
// -----------------------------------------------------------------------------

def SET : Inst
{
  let Fields = [
    RegField<"Reg">,
    InstField<"Value">
  ];
}

def SYSCALL : Inst
{
  let Fields = [
    InstField<"Syscall">,
    InstListField<"Args">,
  ];
  let NumTypes = -1;
}

def CLONE : Inst
{
  let Fields = [
      InstField<"Callee">,
      InstField<"Stack">,
      InstField<"Flags">,
      InstField<"Arg">,
      InstField<"PTID">,
      InstField<"TLS">,
      InstField<"CTID">,
  ];
  let NumTypes = 1;
}

// -----------------------------------------------------------------------------
// X86 instructions
// -----------------------------------------------------------------------------

def X86_XCHG : Memory
{
  let Fields = [ InstField<"Addr">, InstField<"Val"> ];
  let NumTypes = 1;
}

def X86_CMP_XCHG : Memory
{
  let Fields = [ InstField<"Addr">, InstField<"Val">, InstField<"Ref"> ];
  let NumTypes = 1;
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
  let NumTypes = 1;
}

def X86_CPUID : Inst
{
  let Type = "X86_CPUID";
  let Fields = [ InstField<"Leaf">, InstField<"Subleaf"> ];
  let NumTypes = -1;
}

class X86_FPUControlInst : Inst
{
  let Fields = [ InstField<"Addr"> ];
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
  let Fields = [ InstField<"Addr"> ];
  let NumTypes = 1;
}

def AARCH64_SC : Memory
{
  let Type = "AArch64_SC";
  let Fields = [ InstField<"Addr">, InstField<"Value"> ];
  let NumTypes = 1;
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
  let Fields = [ InstField<"Addr">, InstField<"Val"> ];
  let NumTypes = 1;
}

def RISCV_CMP_XCHG : Memory
{
  let Fields = [ InstField<"Addr">, InstField<"Val">, InstField<"Ref"> ];
  let NumTypes = 1;
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
  let Fields = [ InstField<"Addr"> ];
  let NumTypes = 1;
}

def PPC_SC : Inst
{
  let Type = "PPC_SC";
  let Fields = [ InstField<"Addr">, InstField<"Value"> ];
  let NumTypes = 1;
}

def PPC_SYNC : Inst
{
  let Type = "PPC_Sync";
}

def PPC_ISYNC : Inst
{
  let Type = "PPC_ISync";
}
