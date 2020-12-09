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

class OptInstField<string name> : Field {
  let Name = name;
  let Type = "Inst";
  let IsOptional = 1;
}

class InstListField<string name> : Field {
  let Name = name;
  let IsList = 1;
  let Type = "Inst";
}

class FlagListField<string name> : Field {
  let Name = name;
  let IsList = 1;
  let IsScalar = 1;
  let Type = "TypeFlag";
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

// -----------------------------------------------------------------------------
// Base classes in the instruction hierarchy.
// -----------------------------------------------------------------------------

/**
 * Base class for the instruction hierarchy.
 */
class Inst {
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

  bit IsReturn = 0;
  bit IsConstant = 0;
  bit IsTerminator = 0;
  bit HasSideEffects = 0;
}

class OperatorInst : Inst {
  let NumTypes = 1;
}

class UnaryInst : OperatorInst {
  let Fields = [ InstField<"Arg"> ];
}

class ConversionInst : UnaryInst {
}

class BinaryInst : OperatorInst {
  let Fields = [ InstField<"LHS">, InstField<"RHS"> ];
  bit IsReturn = 0;
  bit IsConstant = 0;
  bit HasSideEffects = 0;
}

class OverflowInst : BinaryInst {
}

class ShiftRotateInst : BinaryInst {
}

class RotateInst : ShiftRotateInst {
}

class ShiftInst : ShiftRotateInst {
}

class DivisionInst : BinaryInst {
}

class MemoryInst : Inst {
}

class LoadLinkInst : MemoryInst {
  let Fields = [ InstField<"Addr"> ];
  let NumTypes = 1;
}

class StoreCondInst : MemoryInst {
  let Fields = [ InstField<"Addr">, InstField<"Value"> ];
  let NumTypes = 1;
  let HasSideEffects = 1;
}

class ExchangeInst : MemoryInst {
  let Fields = [ InstField<"Addr">, InstField<"Val"> ];
  let NumTypes = 1;
  bit HasSideEffects = 1;
}

class CompareExchangeInst : MemoryInst {
  let Fields = [ InstField<"Addr">, InstField<"Val">, InstField<"Ref"> ];
  let NumTypes = 1;
  bit HasSideEffects = 1;
}

class BarrierInst : MemoryInst {
  bit HasSideEffects = 1;
}

class ConstInst : OperatorInst {
  let IsConstant = 1;
}

class ControlInst : Inst {
}

class TerminatorInst : ControlInst {
  bit IsTerminator = 1;

  int NumSuccessors = 0;
}

// -----------------------------------------------------------------------------
// PHI instruction.
// -----------------------------------------------------------------------------

def PHI : Inst
{
  bit HasCustomClone = 1;
  bit HasCustomReader = 1;
  bit HasCustomWriter = 1;
  bit HasCustomDefinition = 1;
  let HasCustomPrinter = 1;
}

// -----------------------------------------------------------------------------
// ControlInst flow.
// -----------------------------------------------------------------------------

class CallSite : TerminatorInst {
  let Fields = [
    InstField<"Callee">,
    InstListField<"Args">,
    FlagListField<"Flags">,
    OptUnsignedField<"NumFixedArgs">,
    CallingConvField<"CallingConv">
  ];
  let HasSideEffects = 1;
  let HasCustomDefinition = 1;
}

def CALL : CallSite
{
  let Fields = [
    InstField<"Callee">,
    InstListField<"Args">,
    FlagListField<"Flags">,
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
    FlagListField<"Flags">,
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
    FlagListField<"Flags">,
    BlockField<"Cont">,
    BlockField<"Throw">,
    OptUnsignedField<"NumFixedArgs">,
    CallingConvField<"CallingConv">
  ];
  let NumTypes = -1;
}

def RETURN : TerminatorInst
{
  let Fields = [
    InstListField<"Args">
  ];
  let HasCustomDefinition = 1;
}

def JUMP_COND : TerminatorInst
{
  let Fields = [
    InstField<"Cond">,
    BlockField<"TrueTarget">,
    BlockField<"FalseTarget">
  ];
  let HasCustomDefinition = 1;
}

def JUMP : TerminatorInst
{
  let Fields = [ BlockField<"Target"> ];
  let HasCustomDefinition = 1;
}

def SWITCH : TerminatorInst
{
  let Fields = [
    InstField<"Idx">,
    BlockListField<"Blocks">
  ];
  let HasCustomDefinition = 1;
}

/**
 * Trap instruction which terminates a block.
 *
 * The trap instruction should never be reached by execution. It lowers to
 * an illegal instruction to aid debugging.
 */
def TRAP : TerminatorInst
{
  let NumSuccessors = 0;
}

def RAISE : TerminatorInst
{
  let Fields = [
    OptCallinvConvField<"CallingConv">,
    InstField<"Target">,
    InstField<"Stack">,
    InstListField<"Args">
  ];
  let HasCustomDefinition = 1;
}

/**
 * Landing pad instruction for exception handling.
 *
 * Introduces values transferred from the raise site through registers.
 * Control to a landing pad should be transferred to from a different
 * function down the call stack using the raise instruction. If the
 * landing pad introduces values, a calling convention is required to
 * map values to physical registers.
 */
def LANDING_PAD : ControlInst
{
  let Fields = [ OptCallinvConvField<"CallingConv"> ];
  let NumTypes = -1;
  let HasSideEffects = 1;
}

// -----------------------------------------------------------------------------
// MemoryInst.
// -----------------------------------------------------------------------------

def LOAD : MemoryInst
{
  let Fields = [ InstField<"Addr"> ];
  let NumTypes = 1;
}

def STORE : MemoryInst
{
  let Fields = [ InstField<"Addr">, InstField<"Val"> ];
  bit HasSideEffects = 1;
}

// -----------------------------------------------------------------------------
// Varargs.
// -----------------------------------------------------------------------------

def VA_START : Inst
{
  let Fields = [ InstField<"VAList"> ];
  let HasSideEffects = 1;
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

def ARG : ConstInst
{
  let Fields = [UnsignedField<"Idx">];
  let NumTypes = 1;
}

def FRAME : ConstInst
{
  let Fields = [
    UnsignedField<"Object">,
    UnsignedField<"Offset">
  ];
  let NumTypes = 1;
  let IsConstant = 1;
}

/**
 * Undefined value.
 *
 * Undefined values are aggressively propagated and eliminated.
 * Lowers to ISD::UNDEF, allowing LLVM to further propagate it.
 */
def UNDEF : ConstInst
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
def MOV : OperatorInst
{
  let Fields = [ ValueField<"Arg"> ];
  let NumTypes = 1;
  bit HasCustomDefinition = 1;
}

// -----------------------------------------------------------------------------
// UnaryInst instructions
// -----------------------------------------------------------------------------

def S_EXT : ConversionInst;
def Z_EXT : ConversionInst;
def F_EXT : ConversionInst;
def X_EXT : ConversionInst;
def TRUNC : ConversionInst;

def ABS : UnaryInst;
def NEG : UnaryInst;
def SQRT : UnaryInst;
def SIN : UnaryInst;
def COS : UnaryInst;
def EXP : UnaryInst;
def EXP2 : UnaryInst;
def LOG : UnaryInst;
def LOG2 : UnaryInst;
def LOG10 : UnaryInst;
def F_CEIL : UnaryInst;
def F_FLOOR : UnaryInst;

def POP_COUNT : UnaryInst;
def CLZ : UnaryInst;
def CTZ : UnaryInst;
def BYTE_SWAP : UnaryInst;

// -----------------------------------------------------------------------------
// BinaryInst Instructions
// -----------------------------------------------------------------------------
def ADD : BinaryInst;
def AND : BinaryInst;
def SUB : BinaryInst;
def XOR : BinaryInst;
def POW : BinaryInst;
def MUL : BinaryInst;
def OR : BinaryInst;

def U_DIV : DivisionInst;
def S_DIV : DivisionInst;
def U_REM : DivisionInst;
def S_REM : DivisionInst;

def ROTL : RotateInst;
def ROTR : RotateInst;

def SLL : ShiftInst;
def SRA : ShiftInst;
def SRL : ShiftInst;

def COPY_SIGN : BinaryInst;

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
def O_U_ADD : OverflowInst;
def O_U_MUL : OverflowInst;
def O_U_SUB : OverflowInst;
def O_S_ADD : OverflowInst;
def O_S_MUL : OverflowInst;
def O_S_SUB : OverflowInst;

// -----------------------------------------------------------------------------
// Generic hardware instructions.
// -----------------------------------------------------------------------------

def SET : Inst
{
  let Fields = [
    RegField<"Reg">,
    InstField<"Value">
  ];

  bit HasSideEffects = 1;
}

def SYSCALL : Inst
{
  let Fields = [
    InstField<"Syscall">,
    InstListField<"Args">,
  ];
  let NumTypes = -1;

  bit HasCustomDefinition = 1;
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
  let HasSideEffects = 1;
}

// -----------------------------------------------------------------------------
// X86 instructions
// -----------------------------------------------------------------------------

def X86_XCHG : ExchangeInst;

def X86_CMP_XCHG : CompareExchangeInst;

def X86_D_FENCE : BarrierInst;

def X86_FN_CL_EX : Inst
{
  bit HasSideEffects = 1;
}

def X86_RD_TSC : Inst
{
  let NumTypes = 1;
}

def X86_CPU_ID : Inst
{
  let Fields = [ InstField<"Leaf">, OptInstField<"Subleaf"> ];
  let NumTypes = -1;
  let IsConstant = 1;
}

class X86_FPUControlInst : Inst
{
  let Fields = [ InstField<"Addr"> ];
  bit HasSideEffects = 1;
}

def X86_FN_ST_CW : X86_FPUControlInst;
def X86_FN_ST_SW : X86_FPUControlInst;
def X86_FN_ST_ENV : X86_FPUControlInst;
def X86_F_LD_CW : X86_FPUControlInst;
def X86_F_LD_ENV : X86_FPUControlInst;
def X86_LDM_XCSR : X86_FPUControlInst;
def X86_STM_XCSR : X86_FPUControlInst;

def X86_IN : Inst
{
  let Fields = [ InstField<"Port"> ];
  let NumTypes = 1;
  let HasSideEffects = 1;
}

def X86_OUT : Inst
{
  let Fields = [ InstField<"Port">, InstField<"Value"> ];
  let HasSideEffects = 1;
}

def X86_WR_MSR : Inst
{
  let Fields = [ InstField<"Reg">, InstField<"Lo">, InstField<"Hi"> ];
  let HasSideEffects = 1;
}

// -----------------------------------------------------------------------------
// AArch64 instructions
// -----------------------------------------------------------------------------

def AARCH64_LOAD_LINK : LoadLinkInst;

def AARCH64_STORE_COND : StoreCondInst;

def AARCH64_D_FENCE : BarrierInst;

// -----------------------------------------------------------------------------
// RISC-V instructions
// -----------------------------------------------------------------------------

def RISCV_XCHG : ExchangeInst;

def RISCV_CMP_XCHG : CompareExchangeInst;

def RISCV_FENCE : BarrierInst;

def RISCV_GP : Inst
{
  bit HasSideEffects = 1;
}

// -----------------------------------------------------------------------------
// Power instructions
// -----------------------------------------------------------------------------

def PPC_LOAD_LINK : LoadLinkInst;

def PPC_STORE_COND : StoreCondInst;

def PPC_FENCE : BarrierInst;

def PPC_I_FENCE : BarrierInst;
