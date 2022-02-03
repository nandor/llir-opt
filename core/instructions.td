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
  bit IsUnwrapped = 0;
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
  let IsUnwrapped = 1;
}

class BlockListField<string name> : Field {
  let Name = name;
  let IsList = 1;
  let Type = "Block";
  let IsUnwrapped = 1;
}

class IntField<string name> : Field {
  let Name = name;
  let IsScalar = 1;
  let Type = "int";
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
  // Custom comparison required.
  bit HasCustomCompare = 0;
  // Custom parsing required.
  bit HasCustomParser = 0;
  // Number of types attached to the instruction.
  int NumTypes = 0;
  // Number of returned values, -1 meaning equal to the number of types.
  int NumReturns = -1;

  /// Indicates whether the instruction can return from the function.
  bit IsReturn = 0;
  /// Indicates whether the instruction introduces a constant.
  bit IsConstant = 0;
  /// Indicates basic block terminators.
  bit IsTerminator = 0;
  /// The instruction produces side effects on memory or hardware state.
  bit HasSideEffects = 0;

  /// Custom code in the interface.
  string CustomIntf = "";
}

class OperatorInst : Inst {
  let NumTypes = 1;
}

class UnaryInst : OperatorInst {
  let Fields = [ InstField<"Arg"> ];
}

class BitCountInst : UnaryInst {
}

class ConversionInst : UnaryInst {
}

class FloatInst : UnaryInst {
}

class ExtensionInst : ConversionInst {
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

class ShiftRightInst : ShiftInst {
}

class MultiplyInst : BinaryInst {
}

class DivisionRemainderInst : BinaryInst {
}

class DivisionInst : DivisionRemainderInst {
}

class RemainderInst : DivisionRemainderInst {
}

class MemoryInst : Inst {
}

class MemoryLoadInst : MemoryInst {
  let Fields = [ InstField<"Addr"> ];
  let NumTypes = 1;
}

class MemoryStoreInst : MemoryInst {
  let Fields = [ InstField<"Addr">, InstField<"Value"> ];
  bit HasSideEffects = 1;
}

class LoadLinkInst : MemoryLoadInst {
  let Fields = [ InstField<"Addr"> ];
  let NumTypes = 1;
}

class StoreCondInst : MemoryStoreInst {
  let NumTypes = 1;
}

class MemoryExchangeInst : MemoryInst {
  let Fields = [ InstField<"Addr">, InstField<"Value"> ];
  let NumTypes = 1;
  let HasSideEffects = 1;
}

class MemoryCompareExchangeInst : MemoryExchangeInst {
  let Fields = [ InstField<"Addr">, InstField<"Value">, InstField<"Ref"> ];
  let NumTypes = 1;
  let HasSideEffects = 1;
}

class BarrierInst : MemoryInst {
  let HasSideEffects = 1;
}

class ConstInst : OperatorInst {
  let IsConstant = 1;
}

class ControlInst : Inst {
}

class YieldInst : Inst {
}

class TerminatorInst : ControlInst {
  let IsTerminator = 1;
}

class StopInst : TerminatorInst {
  bit HasSideEffects = 1;
}

// -----------------------------------------------------------------------------
// PHI instruction.
// -----------------------------------------------------------------------------

def PHI : Inst
{
  let HasCustomClone = 1;
  let HasCustomReader = 1;
  let HasCustomWriter = 1;
  let HasCustomDefinition = 1;
  let HasCustomPrinter = 1;
  let HasCustomCompare = 1;
  let HasCustomParser = 1;
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
  let NumTypes = -1;

  let CustomIntf = [{
    /// Returns the called func.
    Func *GetDirectCallee();
    /// Returns the called func.
    const Func *GetDirectCallee() const
    {
      return const_cast<CallSite *>(this)->GetDirectCallee();
    }

    /// Changes the calling conv.
    void SetCallingConv(CallingConv conv) { CallingConv_ = conv; }

    /// Checks if the function is var arg: more args than fixed ones.
    bool IsVarArg() const { return !!GetNumFixedArgs(); }
  }];

  let HasCustomParser = 1;
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
  let IsReturn = 1;
  let NumReturns = 0;
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
}

/**
 * Return to a call site from a function.
 */
def RETURN : TerminatorInst
{
  let Fields = [
    InstListField<"Args">
  ];
  let HasSideEffects = 1;
  let IsReturn = 1;

  let HasCustomParser = 1;
}


/**
 * Conditional jump instruction.
 *
 * Accepts a flag. If the argument is zero, the false branch is taken,
 * otherwise the true branch is taken.
 */
def JUMP_COND : TerminatorInst
{
  let Fields = [
    InstField<"Cond">,
    BlockField<"TrueTarget">,
    BlockField<"FalseTarget">
  ];
}

/**
 * Unconditional jump instruction.
 *
 * Transfers control to a basic block in the same function.
 */
def JUMP : TerminatorInst
{
  let Fields = [ BlockField<"Target"> ];
}

/**
 * Switch instruction
 *
 * Lowers to an efficient jump table. Takes a control index argument,
 * along with a table of successor blocks. If the control index is out of
 * bounds, behaviour is undefined.
 */
def SWITCH : TerminatorInst
{
  let Fields = [
    InstField<"Index">,
    BlockListField<"Blocks">
  ];

  let HasCustomParser = 1;
}

/**
 * Trap instruction which terminates a block.
 *
 * The trap instruction should never be reached by execution. It lowers to
 * an illegal instruction to aid debugging.
 */
def TRAP : StopInst;

/**
 * Debug trap instruction invoking a debugger.
 */
def DEBUG_TRAP : StopInst;

/**
 * Long jump instruction.
 *
 * Used to implement longjmp: transfers control to the program point after the
 * setjmp call. The arguments include the target basic block, the stack pointer
 * to reset to and the value to return from the setjmp call.
 */
def RAISE : TerminatorInst
{
  let Fields = [
    OptCallinvConvField<"CallingConv">,
    InstField<"Target">,
    InstField<"Stack">,
    InstListField<"Args">
  ];
  bit HasSideEffects = 1;

  let HasCustomParser = 1;
}

/**
 * Context switch instruction, to transfer control to a new thread.
 */
def SPAWN : TerminatorInst
{
  let Fields = [
    InstField<"Stack">,
    InstField<"Target">
  ];
  bit HasSideEffects = 1;
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

  let HasCustomParser = 1;
}

// -----------------------------------------------------------------------------
// MemoryInst.
// -----------------------------------------------------------------------------

def LOAD : MemoryLoadInst;

def STORE : MemoryStoreInst;

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
  let Fields = [UnsignedField<"Index">];
  let NumTypes = 1;
}

def FRAME : ConstInst
{
  let Fields = [
    UnsignedField<"Object">,
    IntField<"Offset">
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

def S_EXT : ExtensionInst;
def Z_EXT : ExtensionInst;
def F_EXT : ExtensionInst;
def X_EXT : ExtensionInst;
def TRUNC : ConversionInst;
def BIT_CAST : ConversionInst;

def ABS : FloatInst;
def NEG : FloatInst;
def SQRT : FloatInst;
def SIN : FloatInst;
def COS : FloatInst;
def EXP : FloatInst;
def EXP2 : FloatInst;
def LOG : FloatInst;
def LOG2 : FloatInst;
def LOG10 : FloatInst;
def F_CEIL : FloatInst;
def F_FLOOR : FloatInst;

def POP_COUNT : BitCountInst;
def CLZ : BitCountInst;
def CTZ : BitCountInst;
def BYTE_SWAP : UnaryInst;

// -----------------------------------------------------------------------------
// BinaryInst Instructions
// -----------------------------------------------------------------------------

def MUL : MultiplyInst;
def MUL_H_S : MultiplyInst;
def MUL_H_U : MultiplyInst;

def ADD : BinaryInst;
def AND : BinaryInst;
def SUB : BinaryInst;
def XOR : BinaryInst;
def POW : BinaryInst;
def OR : BinaryInst;

def U_DIV : DivisionInst;
def S_DIV : DivisionInst;
def U_REM : RemainderInst;
def S_REM : RemainderInst;

def ROTL : RotateInst;
def ROTR : RotateInst;

def SLL : ShiftInst;
def SRA : ShiftRightInst;
def SRL : ShiftRightInst;

def COPY_SIGN : BinaryInst;

def CMP : BinaryInst {
  let Fields = [
    InstField<"LHS">,
    InstField<"RHS">,
    CondCodeField<"CC">,
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
  let Fields = [ RegField<"Reg">, InstField<"Value"> ];
  let HasSideEffects = 1;
}

def GET : OperatorInst
{
  let Fields = [ RegField<"Reg"> ];
  let HasSideEffects = 1;
}

def SYSCALL : Inst
{
  let Fields = [
    InstField<"Syscall">,
    InstListField<"Args">,
  ];
  let NumTypes = -1;

  bit HasSideEffects = 1;

  let HasCustomParser = 1;
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

def X86_XCHG : MemoryExchangeInst;

def X86_CMP_XCHG : MemoryCompareExchangeInst;

def X86_M_FENCE : BarrierInst;
def X86_L_FENCE : BarrierInst;
def X86_S_FENCE : BarrierInst;
def X86_BARRIER : BarrierInst;

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
  let NumTypes = 4;
  let IsConstant = 1;

  let HasCustomParser = 1;
}

def X86_GET_XCR : Inst
{
  let Fields = [ InstField<"Reg"> ];
  let NumTypes = 2;
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

def X86_RD_MSR : Inst
{
  let Fields = [ InstField<"Reg"> ];
  let NumTypes = 2;
}

def X86_PAUSE : Inst
{
  let HasSideEffects = 1;
}

def X86_YIELD : YieldInst
{
  let HasSideEffects = 1;
}

def X86_STI : Inst
{
  let HasSideEffects = 1;
}

def X86_CLI : Inst
{
  let HasSideEffects = 1;
}

def X86_HLT : Inst
{
  let HasSideEffects = 1;
}

def X86_NOP : Inst
{
  let HasSideEffects = 1;
}

def X86_SPIN : Inst
{
  let HasSideEffects = 1;
}

def X86_LGDT : Inst
{
  let Fields = [ InstField<"Value"> ];
  let HasSideEffects = 1;
}

def X86_LIDT : Inst
{
  let Fields = [ InstField<"Value"> ];
  let HasSideEffects = 1;
}

def X86_LTR : Inst
{
  let Fields = [ InstField<"Value"> ];
  let HasSideEffects = 1;
}

class X86_ContextInst : Inst
{
  let Fields = [ InstField<"Addr"> ];
  bit HasSideEffects = 1;
}

def X86_F_SAVE : X86_ContextInst
{
  let Fields = [ InstField<"Addr"> ];
  let HasSideEffects = 1;
}

def X86_F_RESTORE : X86_ContextInst
{
  let Fields = [ InstField<"Addr"> ];
  let HasSideEffects = 1;
}

def X86_F_X_SAVE : X86_ContextInst
{
  let Fields = [ InstField<"Addr"> ];
  let HasSideEffects = 1;
}

def X86_F_X_RESTORE : X86_ContextInst
{
  let Fields = [ InstField<"Addr"> ];
  let HasSideEffects = 1;
}

def X86_X_SAVE : X86_ContextInst
{
  let Fields = [ InstField<"Addr">, InstField<"RFBM"> ];
  let HasSideEffects = 1;
}

def X86_X_SAVE_OPT : X86_ContextInst
{
  let Fields = [ InstField<"Addr">, InstField<"RFBM"> ];
  let HasSideEffects = 1;
}

def X86_X_RESTORE : X86_ContextInst
{
  let Fields = [ InstField<"Addr">, InstField<"RFBM"> ];
  let HasSideEffects = 1;
}

def X86_INT : Inst
{
  let Fields = [
    UnsignedField<"Interrupt">,
    InstListField<"Args">
  ];
  let HasSideEffects = 1;

  let HasCustomParser = 1;
}

// -----------------------------------------------------------------------------
// AArch64 instructions
// -----------------------------------------------------------------------------

def AARCH64_LOAD_LINK : LoadLinkInst;

def AARCH64_STORE_COND : StoreCondInst;

def AARCH64_DMB : BarrierInst;

def AARCH64_ISB : BarrierInst;

def AARCH64_WFI : Inst
{
  let HasSideEffects = 1;
}

def AARCH64_STI : Inst
{
  let HasSideEffects = 1;
}

def AARCH64_CLI : Inst
{
  let HasSideEffects = 1;
}

def AARCH64_OUT : Inst
{
  let Fields = [ InstField<"Port">, InstField<"Value"> ];
  let HasSideEffects = 1;
}

// -----------------------------------------------------------------------------
// RISC-V instructions
// -----------------------------------------------------------------------------

def RISCV_XCHG : MemoryExchangeInst;

def RISCV_CMP_XCHG : MemoryCompareExchangeInst;

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
