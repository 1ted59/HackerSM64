#include <ultra64.h>

#include <string.h>

#include "types.h"
#include "sm64.h"

#include "crash_screen/crash_draw.h"
#include "crash_screen/crash_main.h"
#include "crash_screen/crash_settings.h"
#include "crash_screen/crash_pages.h"
#include "crash_screen/crash_print.h"
#include "map_parser.h"
#include "registers.h"

#include "insn_disasm.h"

#include "crash_screen/pages/page_disasm.h"

#include "engine/math_util.h"


/**
 * How to find instructions:
 *
 * - If first 4 bits (cop_opcode) == 0100 (COP_OPCODE):
 *  - The next 2 bits (cop_num) are coprocessor number, so use insn_db_cop_lists
 *  - compare the next 2 bits (cop_subtype):
 *   - if 0:
 *    - compare the next 3 bits (fmt) to the list
 *   - otherwise, if 1:
 *    - skip the next 3 bits (fmt) and compare the 5 bits after that (ft) to the list
 *   - otherwise, if 2:
 *    - compare the last 6 bits (func) to the list
 *   - otherwise, if 3:
 *    - invalid/unknown instruction.
 * - otherwise, check first 6 bits (opcode)
 *  - if 0 (OPC_SPECIAL):
 *   - compare the last 6 bits (func) to insn_db_spec
 *  - otherwise, if 1 (OPC_REGIMM):
 *   - skip the next 5 bits and compare the 5 bits after that (regimm)
 *  - otherwise, compare the first 6 bits to insn_db_standard
 */


// MIPS III Instructions:

// Opcode instructions:
ALIGNED32 static const InsnTemplate insn_db_standard[] = { // INSN_TYPE_OPCODE
    // OPC_SPECIAL (insn_db_spec) // 0: OPC_SPECIAL, INSN_TYPE_FUNC
    // OPC_REGIMM  (insn_db_regi) // 1: OPC_REGIMM,  INSN_TYPE_REGIMM
    { .name = "J"      , .fmt = "\'J"  , .out = 0, .opcode = OPC_J           }, //  2: Jump.
    { .name = "JAL"    , .fmt = "\'J"  , .out = 0, .opcode = OPC_JAL         }, //  3: Jump and Link.
    { .name = "BEQ"    , .fmt = "\'stB", .out = 0, .opcode = OPC_BEQ         }, //  4: Branch on Equal.
    { .name = "BNE"    , .fmt = "\'stB", .out = 0, .opcode = OPC_BNE         }, //  5: Branch on Not Equal.
    { .name = "BLEZ"   , .fmt = "\'sB" , .out = 0, .opcode = OPC_BLEZ        }, //  6: Branch on Less Than or Equal to Zero.
    { .name = "BGTZ"   , .fmt = "\'sB" , .out = 0, .opcode = OPC_BGTZ        }, //  7: Branch on Greater Than Zero.
    { .name = "ADDI"   , .fmt = "\'tsI", .out = 1, .opcode = OPC_ADDI        }, //  8: Add Immediate Word.
    { .name = "ADDIU"  , .fmt = "\'tsI", .out = 1, .opcode = OPC_ADDIU       }, //  9: Add Immediate Unsigned Word.
    { .name = "SLTI"   , .fmt = "\'tsI", .out = 1, .opcode = OPC_SLTI        }, // 10: Set on Less Than Immediate.
    { .name = "SLTIU"  , .fmt = "\'tsI", .out = 1, .opcode = OPC_SLTIU       }, // 11: Set on Less Than Immediate Unsigned.
    { .name = "ANDI"   , .fmt = "\'tsI", .out = 1, .opcode = OPC_ANDI        }, // 12: And Immediate.
    { .name = "ORI"    , .fmt = "\'tsI", .out = 1, .opcode = OPC_ORI         }, // 13: Or Immediate.
    { .name = "XORI"   , .fmt = "\'tsI", .out = 1, .opcode = OPC_XORI        }, // 14: Exclusive Or Immediate.
    { .name = "LUI"    , .fmt = "\'tI" , .out = 1, .opcode = OPC_LUI         }, // 15: Load Upper Immediate.
    // OPC_COP0 (insn_db_cop0) // 16: Coprocessor-0 (System Control Coprocessor).
    // OPC_COP1 (insn_db_cop1) // 17: Coprocessor-1 (Floating-Point Unit).
    // OPC_COP2 (insn_db_cop2) // 18: Coprocessor-2 (Reality Co-Processor Vector Unit).
    // OPC_COP3 (insn_db_cop3) // 19: Coprocessor-3 (CP3).
    { .name = "BEQL"   , .fmt = "\'stB", .out = 0, .opcode = OPC_BEQL        }, // 20: Branch on Equal Likely.
    { .name = "BNEL"   , .fmt = "\'stB", .out = 0, .opcode = OPC_BNEL        }, // 21: Branch on Not Equal Likely.
    { .name = "BLEZL"  , .fmt = "\'sB" , .out = 0, .opcode = OPC_BLEZL       }, // 22: Branch on Less Than or Equal to Zero Likely.
    { .name = "BGTZL"  , .fmt = "\'sB" , .out = 0, .opcode = OPC_BGTZL       }, // 23: Branch on Greater Than Zero Likely.
    { .name = "DADDI"  , .fmt = "\'tsI", .out = 1, .opcode = OPC_DADDI       }, // 24: Doubleword Add Immediate.
    { .name = "DADDIU" , .fmt = "\'tsI", .out = 1, .opcode = OPC_DADDIU      }, // 25: Doubleword Add Immediate Unsigned.
    { .name = "LDL"    , .fmt = "\'tI(", .out = 1, .opcode = OPC_LDL         }, // 26: Load Doubleword Left.
    { .name = "LDR"    , .fmt = "\'tI(", .out = 1, .opcode = OPC_LDR         }, // 27: Load Doubleword Right.
    { .name = "LB"     , .fmt = "\'tI(", .out = 1, .opcode = OPC_LB          }, // 32: Load Byte.
    { .name = "LH"     , .fmt = "\'tI(", .out = 1, .opcode = OPC_LH          }, // 33: Load Halfword.
    { .name = "LWL"    , .fmt = "\'tI(", .out = 1, .opcode = OPC_LWL         }, // 34: Load Word Left.
    { .name = "LW"     , .fmt = "\'tI(", .out = 1, .opcode = OPC_LW          }, // 35: Load Word.
    { .name = "LBU"    , .fmt = "\'tI(", .out = 1, .opcode = OPC_LBU         }, // 36: Load Byte Unsigned.
    { .name = "LHU"    , .fmt = "\'tI(", .out = 1, .opcode = OPC_LHU         }, // 37: Load Halfword Unsigned.
    { .name = "LWR"    , .fmt = "\'tI(", .out = 1, .opcode = OPC_LWR         }, // 38: Load Word Right.
    { .name = "LWU"    , .fmt = "\'tI(", .out = 1, .opcode = OPC_LWU         }, // 39: Load Word Unsigned.
    { .name = "SB"     , .fmt = "\'tI(", .out = 0, .opcode = OPC_SB          }, // 40: Store Byte.
    { .name = "SH"     , .fmt = "\'tI(", .out = 0, .opcode = OPC_SH          }, // 41: Store Halfword.
    { .name = "SWL"    , .fmt = "\'tI(", .out = 0, .opcode = OPC_SWL         }, // 42: Store Word Left.
    { .name = "SW"     , .fmt = "\'tI(", .out = 0, .opcode = OPC_SW          }, // 43: Store Word.
    { .name = "SDL"    , .fmt = "\'tI(", .out = 0, .opcode = OPC_SDL         }, // 44: Store Doubleword Left.
    { .name = "SDR"    , .fmt = "\'tI(", .out = 0, .opcode = OPC_SDR         }, // 45: Store Doubleword Right.
    { .name = "SWR"    , .fmt = "\'tI(", .out = 0, .opcode = OPC_SWR         }, // 46: Store Word Right.
    { .name = "CACHE"  , .fmt = "\'tI(", .out = 0, .opcode = OPC_CACHE       }, // 47: https://techpubs.jurassic.nl/manuals/hdwr/developer/R10K_UM/sgi_html/t5.Ver.2.0.book_301.html.
    { .name = "LL"     , .fmt = "\'tI(", .out = 1, .opcode = OPC_LL          }, // 48: Load Linked Word.
    { .name = "LWC1"   , .fmt = "\'TI(", .out = 0, .opcode = OPC_LWC1        }, // 49: Load Word to Coprocessor-1 (Floating-Point Unit).
    { .name = "LWC2"   , .fmt = "\'TI(", .out = 0, .opcode = OPC_LWC2        }, // 50: Load Word to Coprocessor-2 (Reality Co-Processor Vector Unit).
    { .name = "LWC3"   , .fmt = "\'TI(", .out = 0, .opcode = OPC_LWC3        }, // 51: Load Word to Coprocessor-3 (COP3).
    { .name = "LLD"    , .fmt = "\'tI(", .out = 1, .opcode = OPC_LLD         }, // 52: Load Linked Doubleword.
    { .name = "LDC1"   , .fmt = "\'tI(", .out = 1, .opcode = OPC_LDC1        }, // 53: Load Doubleword to Coprocessor-1 (Floating-Point Unit).
    { .name = "LDC2"   , .fmt = "\'tI(", .out = 1, .opcode = OPC_LDC2        }, // 54: Load Doubleword to Coprocessor-2 (Reality Co-Processor Vector Unit).
    { .name = "LD"     , .fmt = "\'tI(", .out = 1, .opcode = OPC_LD          }, // 55: Load Doubleword.
    { .name = "SC"     , .fmt = "\'tI(", .out = 0, .opcode = OPC_SC          }, // 56: Store Conditional Word.
    { .name = "SWC1"   , .fmt = "\'TI(", .out = 0, .opcode = OPC_SWC1        }, // 57: Store Word to Coprocessor-1 (Floating-Point Unit).
    { .name = "SWC2"   , .fmt = "\'TI(", .out = 0, .opcode = OPC_SWC2        }, // 58: Store Word to Coprocessor-2 (Reality Co-Processor Vector Unit).
    { .name = "SWC3"   , .fmt = "\'TI(", .out = 0, .opcode = OPC_SWC3        }, // 59: Store Word to Coprocessor-3 (COP3).
    { .name = "SCD"    , .fmt = "\'tI(", .out = 0, .opcode = OPC_SCD         }, // 60: Store Conditional Doubleword.
    { .name = "SDC1"   , .fmt = "\'tI(", .out = 0, .opcode = OPC_SDC1        }, // 61: Store Doubleword to Coprocessor-1 (Floating-Point Unit).
    { .name = "SDC2"   , .fmt = "\'tI(", .out = 0, .opcode = OPC_SDC2        }, // 62: Store Doubleword to Coprocessor-2 (Reality Co-Processor Vector Unit).
    { .name = "SD"     , .fmt = "\'tI(", .out = 0, .opcode = OPC_SD          }, // 63: Store Doubleword.
    {}, // NULL terminator.
};

// Special opcode instructions:
ALIGNED32 static const InsnTemplate insn_db_spec[] = { // OPC_SPECIAL, INSN_TYPE_FUNC
    { .name = "SLL"    , .fmt = "\'dta", .out = 1, .opcode = OPS_SLL         }, //  0: Shift Word Left Logical.
    { .name = "SRL"    , .fmt = "\'dta", .out = 1, .opcode = OPS_SRL         }, //  2: Shift Word Right Logical.
    { .name = "SRA"    , .fmt = "\'dta", .out = 1, .opcode = OPS_SRA         }, //  3: Shift Word Right Arithmetic.
    { .name = "SLLV"   , .fmt = "\'dts", .out = 1, .opcode = OPS_SLLV        }, //  4: Shift Word Left Logical Variable.
    { .name = "SRLV"   , .fmt = "\'dts", .out = 1, .opcode = OPS_SRLV        }, //  6: Shift Word Right Logical Variable.
    { .name = "SRAV"   , .fmt = "\'dts", .out = 1, .opcode = OPS_SRAV        }, //  7: Shift Word Right Arithmetic Variable.
    { .name = "JR"     , .fmt = "\'s"  , .out = 0, .opcode = OPS_JR          }, //  8: Jump Register.
    { .name = "JALR"   , .fmt = "\'ds" , .out = 0, .opcode = OPS_JALR        }, //  9: Jump and Link Register.
    { .name = "SYSCALL", .fmt = "\'"   , .out = 0, .opcode = OPS_SYSCALL     }, // 12: System Call (assert).
    { .name = "BREAK"  , .fmt = "\'"   , .out = 0, .opcode = OPS_BREAK       }, // 13: Breakpoint.
    { .name = "SYNC"   , .fmt = "\'"   , .out = 0, .opcode = OPS_SYNC        }, // 15: Synchronize Shared Memory.
    { .name = "MFHI"   , .fmt = "\'d"  , .out = 8, .opcode = OPS_MFHI        }, // 16: Move From HI.
    { .name = "MTHI"   , .fmt = "\'s"  , .out = 8, .opcode = OPS_MTHI        }, // 17: Move To HI.
    { .name = "MFLO"   , .fmt = "\'d"  , .out = 8, .opcode = OPS_MFLO        }, // 18: Move From LO.
    { .name = "MTLO"   , .fmt = "\'s"  , .out = 8, .opcode = OPS_MTLO        }, // 19: Move To LO.
    { .name = "DSLLV"  , .fmt = "\'dts", .out = 1, .opcode = OPS_DSLLV       }, // 20: Doubleword Shift Left Logical Variable.
    { .name = "DSRLV"  , .fmt = "\'dts", .out = 1, .opcode = OPS_DSRLV       }, // 22: Doubleword Shift Right Logical Variable.
    { .name = "DSRAV"  , .fmt = "\'dts", .out = 1, .opcode = OPS_DSRAV       }, // 23: Doubleword Shift Right Arithmetic Variable.
    { .name = "MULT"   , .fmt = "\'st" , .out = 8, .opcode = OPS_MULT        }, // 24: Multiply Word (5cyc).
    { .name = "MULTU"  , .fmt = "\'st" , .out = 8, .opcode = OPS_MULTU       }, // 25: Multiply Unsigned Word (5cyc).
    { .name = "DIV"    , .fmt = "\'st" , .out = 8, .opcode = OPS_DIV         }, // 26: Divide Word (37cyc).
    { .name = "DIVU"   , .fmt = "\'st" , .out = 8, .opcode = OPS_DIVU        }, // 27: Divide Unsigned Word (37cyc).
    { .name = "DMULT"  , .fmt = "\'st" , .out = 8, .opcode = OPS_DMULT       }, // 28: Doubleword Multiply (8cyc).
    { .name = "DMULTU" , .fmt = "\'st" , .out = 8, .opcode = OPS_DMULTU      }, // 29: Doubleword Multiply Unsigned (8cyc).
    { .name = "DDIV"   , .fmt = "\'st" , .out = 8, .opcode = OPS_DDIV        }, // 30: Doubleword Divide (69cyc).
    { .name = "DDIVU"  , .fmt = "\'st" , .out = 8, .opcode = OPS_DDIVU       }, // 31: Doubleword Divide Unsigned (69cyc).
    { .name = "ADD"    , .fmt = "\'dst", .out = 1, .opcode = OPS_ADD         }, // 32: Add Word.
    { .name = "ADDU"   , .fmt = "\'dst", .out = 1, .opcode = OPS_ADDU        }, // 33: Add Unsigned Word.
    { .name = "SUB"    , .fmt = "\'dst", .out = 1, .opcode = OPS_SUB         }, // 34: Subtract Word.
    { .name = "SUBU"   , .fmt = "\'dst", .out = 1, .opcode = OPS_SUBU        }, // 35: Subtract Unsigned Word.
    { .name = "AND"    , .fmt = "\'dst", .out = 1, .opcode = OPS_AND         }, // 36: And.
    { .name = "OR"     , .fmt = "\'dst", .out = 1, .opcode = OPS_OR          }, // 37: Or.
    { .name = "XOR"    , .fmt = "\'dst", .out = 1, .opcode = OPS_XOR         }, // 38: Exclusive Or.
    { .name = "NOR"    , .fmt = "\'dst", .out = 1, .opcode = OPS_NOR         }, // 39: Nor.
    { .name = "SLT"    , .fmt = "\'dst", .out = 1, .opcode = OPS_SLT         }, // 42: Set on Less Than.
    { .name = "SLTU"   , .fmt = "\'dst", .out = 1, .opcode = OPS_SLTU        }, // 43: Set on Less Than Unsigned.
    { .name = "DADD"   , .fmt = "\'dst", .out = 1, .opcode = OPS_DADD        }, // 44: Doubleword Add.
    { .name = "DADDU"  , .fmt = "\'dst", .out = 1, .opcode = OPS_DADDU       }, // 45: Doubleword Add Unsigned.
    { .name = "DSUB"   , .fmt = "\'dst", .out = 1, .opcode = OPS_DSUB        }, // 46: Doubleword Subtract.
    { .name = "DSUBU"  , .fmt = "\'dst", .out = 1, .opcode = OPS_DSUBU       }, // 47: Doubleword Subtract Unsigned.
    { .name = "TGE"    , .fmt = "\'st" , .out = 0, .opcode = OPS_TGE         }, // 48: Trap if Greater Than or Equal.
    { .name = "TGEU"   , .fmt = "\'st" , .out = 0, .opcode = OPS_TGEU        }, // 49: Trap if Greater Than or Equal Unsigned.
    { .name = "TLT"    , .fmt = "\'st" , .out = 0, .opcode = OPS_TLT         }, // 50: Trap if Less Than.
    { .name = "TLTU"   , .fmt = "\'st" , .out = 0, .opcode = OPS_TLTU        }, // 51: Trap if Less Than Unsigned.
    { .name = "TEQ"    , .fmt = "\'st" , .out = 0, .opcode = OPS_TEQ         }, // 52: Trap if Equal.
    { .name = "TNE"    , .fmt = "\'st" , .out = 0, .opcode = OPS_TNE         }, // 54: Trap if Not Equal.
    { .name = "DSLL"   , .fmt = "\'dta", .out = 1, .opcode = OPS_DSLL        }, // 56: Doubleword Shift Left Logical.
    { .name = "DSRL"   , .fmt = "\'dta", .out = 1, .opcode = OPS_DSRL        }, // 58: Doubleword Shift Right Logical.
    { .name = "DSRA"   , .fmt = "\'dta", .out = 1, .opcode = OPS_DSRA        }, // 59: Doubleword Shift Right Arithmetic.
    { .name = "DSLL32" , .fmt = "\'dta", .out = 1, .opcode = OPS_DSLL32      }, // 60: Doubleword Shift Left Logical + 32.
    { .name = "DSRL32" , .fmt = "\'dta", .out = 1, .opcode = OPS_DSRL32      }, // 62: Doubleword Shift Right Logical + 32.
    { .name = "DSRA32" , .fmt = "\'dta", .out = 1, .opcode = OPS_DSRA32      }, // 63: Doubleword Shift Right Arithmetic + 32.
    {}, // NULL terminator.
};

// Register opcode instructions:
ALIGNED32 static const InsnTemplate insn_db_regi[] = { // OPC_REGIMM, INSN_TYPE_REGIMM
    { .name = "BLTZ"   , .fmt = "\'sB" , .out = 0, .opcode = OPR_BLTZ        }, //  0: Branch on Less Than Zero.
    { .name = "BGEZ"   , .fmt = "\'sB" , .out = 0, .opcode = OPR_BGEZ        }, //  1: Branch on Greater Than or Equal to Zero.
    { .name = "BLTZL"  , .fmt = "\'sB" , .out = 0, .opcode = OPR_BLTZL       }, //  2: Branch on Less Than Zero Likely.
    { .name = "BGEZL"  , .fmt = "\'sB" , .out = 0, .opcode = OPR_BGEZL       }, //  3: Branch on Greater Than or Equal to Zero Likely.
    { .name = "BLTZAL" , .fmt = "\'sB" , .out = 9, .opcode = OPR_BLTZAL      }, // 16: Branch on Less Than Zero and Link.
    { .name = "BGEZAL" , .fmt = "\'sB" , .out = 9, .opcode = OPR_BGEZAL      }, // 17: Branch on Greater Than or Equal to Zero and Link.
    { .name = "BLTZALL", .fmt = "\'sB" , .out = 9, .opcode = OPR_BLTZALL     }, // 18: Branch on Less Than Zero and Link Likely.
    { .name = "BGEZALL", .fmt = "\'sB" , .out = 9, .opcode = OPR_BGEZALL     }, // 19: Branch on Greater Than or Equal to Zero and Link Likely.
    { .name = "TGEI"   , .fmt = "\'sI" , .out = 0, .opcode = OPR_TGEI        }, //  8: Trap if Greater Than or Equal Immediate.
    { .name = "TGEIU"  , .fmt = "\'sI" , .out = 0, .opcode = OPR_TGEIU       }, //  9: Trap if Greater Than or Equal Unsigned Immediate.
    { .name = "TLTI"   , .fmt = "\'sI" , .out = 0, .opcode = OPR_TLTI        }, // 10: Trap if Less Than Immediate.
    { .name = "TLTIU"  , .fmt = "\'sI" , .out = 0, .opcode = OPR_TLTIU       }, // 11: Trap if Less Than Unsigned Immediate.
    { .name = "TEQI"   , .fmt = "\'sI" , .out = 0, .opcode = OPR_TEQI        }, // 12: Trap if Equal Immediate.
    { .name = "TNEI"   , .fmt = "\'sI" , .out = 0, .opcode = OPR_TNEI        }, // 14: Trap if Not Equal Immediate.
    {}, // NULL terminator.
};

// Coprocessor-0 (System Control Coprocessor):
ALIGNED32 static const InsnTemplate insn_db_cop0_sub00[] = { // OPC_COP0, INSN_TYPE_COP_FMT
    { .name = "MFC0"   , .fmt = "\'t0" , .out = 1, .opcode = COP0_MF         }, //  0: Move from System Control Coprocessor.
    { .name = "DMFC0"  , .fmt = "\'t0" , .out = 1, .opcode = COP0_DMF        }, //  1: Doubleword Move from System Control Coprocessor.
    { .name = "MTC0"   , .fmt = "\'t0" , .out = 2, .opcode = COP0_MT         }, //  4: Move to System Control Coprocessor.
    { .name = "DMTC0"  , .fmt = "\'t0" , .out = 2, .opcode = COP0_DMT        }, //  5: Doubleword Move to System Control Coprocessor.
    {}, // NULL terminator.
};
ALIGNED32 static const InsnTemplate insn_db_cop0_sub10[] = { // OPC_COP0, INSN_TYPE_FUNC
    //! TODO: Find out the proper format for these?
    { .name = "TLBP"   , .fmt = "\'"   , .out = 0, .opcode = OPC_COP0_TLBP   }, //  8: Searches for a TLB entry that matches the EntryHi register.
    { .name = "TLBR"   , .fmt = "\'"   , .out = 0, .opcode = OPC_COP0_TLBR   }, //  1: Loads EntryHi and EntryLo registers with the TLB entry pointed at by the Index register.
    { .name = "TLBWI"  , .fmt = "\'"   , .out = 0, .opcode = OPC_COP0_TLBWI  }, //  2: Stores the contents of EntryHi and EntryLo registers into the TLB entry pointed at by the Index register.
    { .name = "TLBWR"  , .fmt = "\'"   , .out = 0, .opcode = OPC_COP0_TLBWR  }, //  6: Stores the contents of EntryHi and EntryLo registers into the TLB entry pointed at by the Random register.
    { .name = "ERET"   , .fmt = "\'"   , .out = 0, .opcode = OPC_COP0_ERET   }, // 24: Return from interrupt, exception, or error exception.
    {}, // NULL terminator.
};

// Coprocessor-1 (Floating-Point Unit):
ALIGNED32 static const InsnTemplate insn_db_cop1_sub00[] = { // OPC_COP1, INSN_TYPE_COP_FMT
    { .name = "MFC1"   , .fmt = "\'tS" , .out = 1, .opcode = COP1_FMT_SINGLE }, //  0: Move Word From Floating-Point.
    { .name = "DMFC1"  , .fmt = "\'tS" , .out = 1, .opcode = COP1_FMT_DOUBLE }, //  1: Doubleword Move From Floating-Point.
    { .name = "MTC1"   , .fmt = "\'tS" , .out = 2, .opcode = COP1_FMT_WORD   }, //  4: Move Word To Floating-Point.
    { .name = "DMTC1"  , .fmt = "\'tS" , .out = 2, .opcode = COP1_FMT_LONG   }, //  5: Doubleword Move To Floating-Point.
    { .name = "CFC1"   , .fmt = "\'tS" , .out = 1, .opcode = COP1_FMT_CTL_F  }, //  2: Move Control Word From Floating-Point.
    { .name = "CTC1"   , .fmt = "\'tS" , .out = 2, .opcode = COP1_FMT_CTL_T  }, //  6: Move Control Word To Floating-Point.
    {}, // NULL terminator.
};
ALIGNED32 static const InsnTemplate insn_db_cop1_sub01[] = { // OPC_COP1, INSN_TYPE_REGIMM
    { .name = "BC1F"   , .fmt = "\'B"  , .out = 0, .opcode = OPT_COP1_BC1F   }, //  0: Branch on FP False (1cyc*).
    { .name = "BC1T"   , .fmt = "\'B"  , .out = 0, .opcode = OPT_COP1_BC1T   }, //  1: Branch on FP True (1cyc*).
    { .name = "BC1FL"  , .fmt = "\'B"  , .out = 0, .opcode = OPT_COP1_BC1FL  }, //  2: Branch on FP False Likely (1cyc*).
    { .name = "BC1TL"  , .fmt = "\'B"  , .out = 0, .opcode = OPT_COP1_BC1TL  }, //  3: Branch on FP True Likely (1cyc*).
    {}, // NULL terminator.
};
ALIGNED32 static const InsnTemplate insn_db_cop1_sub10[] = { // OPC_COP1, INSN_TYPE_FUNC
    { .name = "ADD"    , .fmt = "\"DST", .out = 1, .opcode = OPS_ADD_F       }, //  0: ADD.[FMT]     Floating-Point Add (3cyc).
    { .name = "SUB"    , .fmt = "\"DST", .out = 1, .opcode = OPS_SUB_F       }, //  1: SUB.[FMT]     Floating-Point Subtract (3cyc).
    { .name = "MUL"    , .fmt = "\"DST", .out = 1, .opcode = OPS_MUL_F       }, //  2: MUL.[FMT]     Floating-Point Multiply (S:5cyc; D:8cyc).
    { .name = "DIV"    , .fmt = "\"DST", .out = 1, .opcode = OPS_DIV_F       }, //  3: DIV.[FMT]     Floating-Point Divide (S:29cyc; D:58cyc).
    { .name = "SQRT"   , .fmt = "\"DS" , .out = 1, .opcode = OPS_SQRT_F      }, //  4: SQRT.[FMT]    Floating-Point Square Root (S:29cyc; D:58cyc).
    { .name = "ABS"    , .fmt = "\"DS" , .out = 1, .opcode = OPS_ABS_F       }, //  5: ABS.[FMT]     Floating-Point Absolute Value (1cyc).
    { .name = "MOV"    , .fmt = "\"DS" , .out = 1, .opcode = OPS_MOV_F       }, //  6: MOV.[FMT]     Floating-Point Move (1cyc).
    { .name = "NEG"    , .fmt = "\"DS" , .out = 1, .opcode = OPS_NEG_F       }, //  7: NEG.[FMT]     Floating-Point Negate (1cyc).
    { .name = "ROUND.L", .fmt = "\"DS" , .out = 1, .opcode = OPS_ROUND_L_F   }, //  8: ROUND.L.[FMT] Floating-Point Round to Long Fixed-Point (5cyc).
    { .name = "TRUNC.L", .fmt = "\"DS" , .out = 1, .opcode = OPS_TRUNC_L_F   }, //  9: TRUNC.L.[FMT] Floating-Point Truncate to Long Fixed-Point (5cyc).
    { .name = "CEIL.L" , .fmt = "\"DS" , .out = 1, .opcode = OPS_CEIL_L_F    }, // 10: CEIL.L.[FMT]  Floating-Point Ceiling to Long Fixed-Point (5cyc).
    { .name = "FLOOR.L", .fmt = "\"DS" , .out = 1, .opcode = OPS_FLOOR_L_F   }, // 11: FLOOR.L.[FMT] Floating-Point Floor to Long Fixed-Point (5cyc).
    { .name = "ROUND.W", .fmt = "\"DS" , .out = 1, .opcode = OPS_ROUND_W_F   }, // 12: ROUND.W.[FMT] Floating-Point Round to Word Fixed-Point (5cyc).
    { .name = "TRUNC.W", .fmt = "\"DS" , .out = 1, .opcode = OPS_TRUNC_W_F   }, // 13: TRUNC.W.[FMT] Floating-Point Truncate to Word Fixed-Point (5cyc).
    { .name = "CEIL.W" , .fmt = "\"DS" , .out = 1, .opcode = OPS_CEIL_W_F    }, // 14: CEIL.W.[FMT]  Floating-Point Ceiling to Word Fixed-Point (5cyc).
    { .name = "FLOOR.W", .fmt = "\"DS" , .out = 1, .opcode = OPS_FLOOR_W_F   }, // 15: FLOOR.W.[FMT] Floating-Point Floor to Word Fixed-Point (5cyc).
    { .name = "CVT.S"  , .fmt = "\"DS" , .out = 1, .opcode = OPS_CVT_S_F     }, // 32: CVT.S.[FMT]   Floating-Point Convert to Single Floating-Point (D:2cyc; W:5cyc; L:5cyc).
    { .name = "CVT.D"  , .fmt = "\"DS" , .out = 1, .opcode = OPS_CVT_D_F     }, // 33: CVT.D.[FMT]   Floating-Point Convert to Double Floating-Point (S:1cyc; W:5cyc; L:5cyc).
    { .name = "CVT.W"  , .fmt = "\"DS" , .out = 1, .opcode = OPS_CVT_W_F     }, // 36: CVT.W.[FMT]   Floating-Point Convert to Word Fixed-Point (5cyc).
    { .name = "CVT.L"  , .fmt = "\"DS" , .out = 1, .opcode = OPS_CVT_L_F     }, // 37: CVT.L.[FMT]   Floating-Point Convert to Long Fixed-Point (5cyc).
    { .name = "C.F"    , .fmt = "\"ST" , .out = 0, .opcode = OPS_C_F         }, // 48: C.F.[FMT]     Floating-Point Compare (False) (1cyc).
    { .name = "C.UN"   , .fmt = "\"ST" , .out = 0, .opcode = OPS_C_UN        }, // 49: C.UN.[FMT]    Floating-Point Compare (Unordered) (1cyc).
    { .name = "C.EQ"   , .fmt = "\"ST" , .out = 0, .opcode = OPS_C_EQ        }, // 50: C.EQ.[FMT]    Floating-point Compare (Equal) (1cyc).
    { .name = "C.UEQ"  , .fmt = "\"ST" , .out = 0, .opcode = OPS_C_UEQ       }, // 51: C.UEQ.[fmt]   Floating-point Compare (Unordered or Equal) (1cyc).
    { .name = "C.OLT"  , .fmt = "\"ST" , .out = 0, .opcode = OPS_C_OLT       }, // 52: C.OLT.[fmt]   Floating-point Compare (Ordered Less Than) (1cyc).
    { .name = "C.ULT"  , .fmt = "\"ST" , .out = 0, .opcode = OPS_C_ULT       }, // 53: C.ULT.[fmt]   Floating-point Compare (Unordered or Less Than) (1cyc).
    { .name = "C.OLE"  , .fmt = "\"ST" , .out = 0, .opcode = OPS_C_OLE       }, // 54: C.OLE.[fmt]   Floating-point Compare (Ordered or Less Than or Equal) (1cyc).
    { .name = "C.ULE"  , .fmt = "\"ST" , .out = 0, .opcode = OPS_C_ULE       }, // 55: C.ULE.[fmt]   Floating-point Compare (Unordered or Less Than or Equal) (1cyc).
    { .name = "C.SF"   , .fmt = "\"ST" , .out = 0, .opcode = OPS_C_SF        }, // 56: C.SF.[fmt]    Floating-point Compare (Signaling False) (1cyc).
    { .name = "C.NGLE" , .fmt = "\"ST" , .out = 0, .opcode = OPS_C_NGLE      }, // 57: C.NGLE.[fmt]  Floating-point Compare (Not Greater or Less Than or Equal) (1cyc).
    { .name = "C.SEQ"  , .fmt = "\"ST" , .out = 0, .opcode = OPS_C_SEQ       }, // 58: C.SEQ.[fmt]   Floating-point Compare (Signalling Equal) (1cyc).
    { .name = "C.NGL"  , .fmt = "\"ST" , .out = 0, .opcode = OPS_C_NGL       }, // 59: C.NGL.[fmt]   Floating-point Compare (Not Greater or Less Than) (1cyc).
    { .name = "C.LT"   , .fmt = "\"ST" , .out = 0, .opcode = OPS_C_LT        }, // 60: C.LT.[fmt]    Floating-point Compare (Less Than) (1cyc).
    { .name = "C.NGE"  , .fmt = "\"ST" , .out = 0, .opcode = OPS_C_NGE       }, // 61: C.NGE.[fmt]   Floating-point Compare (Not Greater Than or Equal) (1cyc).
    { .name = "C.LE"   , .fmt = "\"ST" , .out = 0, .opcode = OPS_C_LE        }, // 62: C.LE.[fmt]    Floating-point Compare (Less Than or Equal) (1cyc).
    { .name = "C.NGT"  , .fmt = "\"ST" , .out = 0, .opcode = OPS_C_NGT       }, // 63: C.NGT.[fmt]   Floating-point Compare (Not Greater Than) (1cyc).
    {}, // NULL terminator.
};

// Coprocessor subtype lists.
static const InsnTemplate* insn_db_cop_lists[][0b11 + 1] = {
    [COP0] = { [INSN_TYPE_COP_FMT] = insn_db_cop0_sub00, [INSN_TYPE_REGIMM] = NULL,               [INSN_TYPE_FUNC] = insn_db_cop0_sub10, [INSN_TYPE_UNKNOWN] = NULL, }, // Coprocessor-0 (System Control Coprocessor).
    [COP1] = { [INSN_TYPE_COP_FMT] = insn_db_cop1_sub00, [INSN_TYPE_REGIMM] = insn_db_cop1_sub01, [INSN_TYPE_FUNC] = insn_db_cop1_sub10, [INSN_TYPE_UNKNOWN] = NULL, }, // Coprocessor-1 (Floating-Point Unit).
    [COP2] = { [INSN_TYPE_COP_FMT] = NULL,               [INSN_TYPE_REGIMM] = NULL,               [INSN_TYPE_FUNC] = NULL,               [INSN_TYPE_UNKNOWN] = NULL, }, // Coprocessor-2 (Reality Co-Processor Vector Unit).
    [COP3] = { [INSN_TYPE_COP_FMT] = NULL,               [INSN_TYPE_REGIMM] = NULL,               [INSN_TYPE_FUNC] = NULL,               [INSN_TYPE_UNKNOWN] = NULL, }, // Coprocessor-3 (CP3).
};

// Pseudo-instructions
ALIGNED32 static const InsnTemplate insn_db_pseudo[] = {
    [PSEUDO_NOP  ] = { .name = "NOP"  , .fmt = "_"    , .out = 0, .opcode = OPS_SLL   }, // NOP (pseudo of SLL).
    [PSEUDO_MOVET] = { .name = "MOVE" , .fmt = "\'dt" , .out = 1, .opcode = OPS_ADD   }, // Move (pseudo of ADD and OR).
    [PSEUDO_MOVES] = { .name = "MOVE" , .fmt = "\'ds" , .out = 1, .opcode = OPS_ADD   }, // Move (pseudo of ADD).
    [PSEUDO_B    ] = { .name = "B"    , .fmt = "\'B"  , .out = 0, .opcode = OPC_BEQ   }, // Branch (pseudo of BEQ).
    [PSEUDO_BEQZ ] = { .name = "BEQZ" , .fmt = "\'sB" , .out = 0, .opcode = OPC_BEQ   }, // Branch on Equal to Zero (pseudo of BEQ).
    [PSEUDO_BNEZ ] = { .name = "BNEZ" , .fmt = "\'sB" , .out = 0, .opcode = OPC_BNE   }, // Branch on Not Equal to Zero (pseudo of BNE).
    [PSEUDO_LI   ] = { .name = "LI"   , .fmt = "\'tI" , .out = 1, .opcode = OPC_ADDI  }, // Load Immediate (pseudo of ADDI and ADDIU).
    [PSEUDO_SUBI ] = { .name = "SUBI" , .fmt = "\'tsi", .out = 1, .opcode = OPC_ADDI  }, // Subtract Immediate Word (pseudo of ADDI).
    [PSEUDO_BEQZL] = { .name = "BEQZL", .fmt = "\'sB" , .out = 0, .opcode = OPC_BEQL  }, // Branch on Equal to Zero Likely (pseudo of BEQL).
    [PSEUDO_BNEZL] = { .name = "BNEZL", .fmt = "\'sB" , .out = 0, .opcode = OPC_BNEL  }, // Branch on Not Equal to Zero Likely (pseudo of BNEL).
    [PSEUDO_DSUBI] = { .name = "DSUBI", .fmt = "\'tsi", .out = 1, .opcode = OPC_DADDI }, // Doubleword Subtract Immediate (pseudo of DADDI).
};


/**
 * @brief Use a specific pseudo-instruction from insn_db_pseudo if 'cond' is TRUE.
 *
 * @param[out] checkInsn A pointer to the InsnTemplate data in insn_db_pseudo matching the given instruction data.
 * @param[in ] id        The index in insn_db_pseudo of the pseudo-instruction template data.
 * @param[in ] cond      Whether to convert the instruction to the pseudo-instruction.
 * @return _Bool Whether the instruction has been converted.
 */
static _Bool check_pseudo_insn(const InsnTemplate** checkInsn, enum PseudoInsns id, _Bool cond) {
    if (cond) {
        *checkInsn = &insn_db_pseudo[id];
        return TRUE;
    }

    return FALSE;
}

/**
 * @brief Checks for pseudo-instructions.
 *
 * @param[out] checkInsn A pointer to the InsnTemplate data in insn_db_pseudo matching the given instruction data.
 * @param[in ] insn      The instruction data that is being read.
 * @return _Bool Whether the instruction has been converted.
 */
static _Bool check_pseudo_instructions(const InsnTemplate** type, InsnData insn) {
    // NOP (trivial case).
    if (check_pseudo_insn(type, PSEUDO_NOP, (insn.raw == 0))) return TRUE;

    // There are no known one-line pseudo-instructions in the Coprocessor lists.
    if (insn.cop_opcode == COP_OPCODE) {
        return FALSE;
    }

    switch (insn.opcode) {
        case OPC_SPECIAL:
            switch (insn.func) {
                case OPS_ADD:
                    if (check_pseudo_insn(type, PSEUDO_MOVES, (insn.rt == 0))) return TRUE;
                    if (check_pseudo_insn(type, PSEUDO_MOVET, (insn.rs == 0))) return TRUE;
                    break;
                case OPS_OR:
                    if (check_pseudo_insn(type, PSEUDO_MOVES, (insn.rt == 0))) return TRUE;
            }
            break;
        case OPC_BEQ:
            if (check_pseudo_insn(type, PSEUDO_B,     (insn.rs == insn.rt))) return TRUE;
            if (check_pseudo_insn(type, PSEUDO_BEQZ,  (insn.rt == 0))) return TRUE;
            break;
        case OPC_BNE:
            if (check_pseudo_insn(type, PSEUDO_BNEZ,  (insn.rt == 0))) return TRUE;
            break;
        case OPC_ADDI:
            if (check_pseudo_insn(type, PSEUDO_LI,    (insn.rs == 0))) return TRUE;
            if (check_pseudo_insn(type, PSEUDO_SUBI,  ((s16)insn.immediate < 0))) return TRUE;
            break;
        case OPC_ADDIU:
            if (check_pseudo_insn(type, PSEUDO_LI,    (insn.rs == 0))) return TRUE;
            break;
        case OPC_BEQL:
            if (check_pseudo_insn(type, PSEUDO_BEQZL, (insn.rt == 0))) return TRUE;
            break;
        case OPC_BNEL:
            if (check_pseudo_insn(type, PSEUDO_BNEZL, (insn.rt == 0))) return TRUE;
            break;
        case OPC_DADDI:
            if (check_pseudo_insn(type, PSEUDO_DSUBI, ((s16)insn.immediate < 0))) return TRUE;
            break;
    }

    return FALSE;
}

/**
 * @brief Gets a pointer to the InsnTemplate data matching the given instruction data.
 *
 * @param[in] insn The instruction data that is being read.
 * @return const InsnTemplate* A pointer to the InsnTemplate data matching the given instruction data.
 */
const InsnTemplate* get_insn(InsnData insn) {
    const InsnTemplate* checkInsn = NULL;
    u8 opcode = insn.opcode; // First 6 bits.

    // Check for pseudo-instructions.
    if (
        cs_get_setting_val(CS_OPT_GROUP_PAGE_DISASM, CS_OPT_DISASM_PSEUDOINSNS) &&
        check_pseudo_instructions(&checkInsn, insn)
    ) {
        return checkInsn;
    }

    // Get the instruction list and opcode to compare.
    switch (insn.opcode) {
        case OPC_COP0:
        case OPC_COP1:
        case OPC_COP2:
        case OPC_COP3:
            checkInsn = insn_db_cop_lists[insn.cop_num][insn.cop_subtype]; // Use COPz lists.
            switch (insn.cop_subtype) {
                case INSN_TYPE_COP_FMT: opcode = insn.fmt;    break; // The 3 bits after the first 8.
                case INSN_TYPE_REGIMM:  opcode = insn.regimm; break; // The 5 bits after the first 11.
                case INSN_TYPE_FUNC:    opcode = insn.func;   break; // Last 6 bits.
                default:                                      break; // 0b11 subtype is unknown and the lists in insn_db_cop_lists are NULL.
            }
            break;
        case OPC_SPECIAL:
            checkInsn = insn_db_spec;
            opcode    = insn.func;
            break;
        case OPC_REGIMM:
            checkInsn = insn_db_regi;
            opcode    = insn.regimm;
            break;
        default:
            checkInsn = insn_db_standard;
            opcode    = insn.opcode;
            break;
    }

    // Loop through the given list.
    while (checkInsn->name != NULL) {
        if (checkInsn->opcode == opcode) {
            return checkInsn;
        }

        checkInsn++;
    }

    return NULL;
}

/**
 * @brief Checks an instruction for its branch offset.
 *
 * @param[in] insn The instruction data that is being read.
 * @return s16 The branch offset. 0 if there is no branch.
 */
s16 insn_check_for_branch_offset(InsnData insn) {
    const InsnTemplate* info = get_insn(insn);

    if ((info != NULL) && strchr(info->fmt, CHAR_P_BRANCH)) {
        return insn.offset;
    }

    return 0x0000;
}

/**
 * @brief Gets the target address of the instruction at 'addr'.
 *
 * @param[in] addr The address of the instruction that is being read.
 * @return Address The target address of the instruction. 'addr' if there is no branch instruction.
 */
Address get_insn_branch_target_from_addr(Address addr) {
    if (!is_in_code_segment(addr)) {
        return addr;
    }

    InsnData insn = {
        .raw = *(Word*)addr,
    };

    // Jump instructions are handled differently than branch instructions.
    if (
        (insn.opcode == OPC_J  ) ||
        (insn.opcode == OPC_JAL)
    ) {
        return PHYSICAL_TO_VIRTUAL(insn.instr_index * sizeof(InsnData));
    }

    s16 branchOffset = insn_check_for_branch_offset(insn);

    if (branchOffset) {
        return (addr + (((s16)branchOffset + 1) * sizeof(InsnData)));
    }

    return addr;
}

/**
 * @brief Gets the corresponding character to print for a Coprocessor-1 (Floating-Point Unit) instruction's format.
 *
 * @param[in] insn The instruction data that is being read.
 * @return char The char value that represents the format in the instruction name.
 */
static char cop1_fmt_to_char(InsnData insn) {
    switch (insn.fmt) {
        case COP1_FMT_SINGLE: return 'S'; // Single
        case COP1_FMT_DOUBLE: return 'D'; // Double
        case COP1_FMT_WORD:   return 'W'; // Word
        case COP1_FMT_LONG:   return 'L'; // Long
    }

    return 'X'; // Unknown
}

/**
 * @brief Prints the new color code if the color has changed.
 *
 * @param[out   ] strp     Pointer to the string.
 * @param[in,out] oldColor The previous color. Gets set to 'newColor' if they are different.
 * @param[in    ] newColor The new color to add to the string.
 */
static void cs_insn_param_check_color_change(char** strp, RGBA32* oldColor, RGBA32 newColor, _Bool format) {
    if (format && (*oldColor != newColor)) {
        *oldColor = newColor;
        *strp += sprintf(*strp, STR_COLOR_PREFIX, newColor);
    }
}


static char insn_as_string[CHAR_BUFFER_SIZE] = "";
static char insn_name[INSN_NAME_DISPLAY_WIDTH] = "";


#define STR_INSN_NAME_BASE      "%s"
#define STR_FORMAT              "%c"
#define STR_CONDITION           "%s"

#define STR_INSN_NAME           "%-"TO_STRING2(INSN_NAME_DISPLAY_WIDTH)"s"
#define STR_INSN_NAME_FORMAT    STR_INSN_NAME_BASE"."STR_FORMAT

#define STR_IREG                "%s"                            // Register
#define STR_IMMEDIATE           STR_HEX_PREFIX STR_HEX_HALFWORD // 0xI
#define STR_OFFSET              "%c"STR_IMMEDIATE               // ±Offset
#define STR_FUNCTION            STR_HEX_PREFIX STR_HEX_WORD     // Function address
#define STR_IREG_BASE           "("STR_IREG")"                  // Base register
#define STR_FREG                "F%02d"                         // Float Register


#define ADD_COLOR(_c) cs_insn_param_check_color_change(&strp, &color, (_c), format);
#define ADD_STR(...) strp += sprintf(strp, __VA_ARGS__);
//! TODO: Actually include the output register in the buffer, but mark it as output.
#define ADD_REG(_fmt, _cop, _idx) {             \
    regInfo = get_reg_info((_cop), (_idx));     \
    ADD_STR((_fmt), regInfo->name);             \
    if (cmdIndex != info->out) {                \
        append_reg_to_buffer((_cop), (_idx));   \
    }                                           \
}


/**
 * @brief Converts MIPS instruction data into a formatted string.
 *
 * @param[in ] addr  The address of the instruction. Used to calculate the target of branches and jumps.
 * @param[in ] insn  The instruction data that is being read.
 * @param[out] fname If the instruction points
 * @return char* The formatted string in insn_as_string.
 */
char* cs_insn_to_string(Address addr, InsnData insn, const char** fname, _Bool format) {
    char* strp = &insn_as_string[0];
    _Bool unimpl = FALSE;

    bzero(insn_as_string, sizeof(insn_as_string));

    const InsnTemplate* info = get_insn(insn);

    if (info != NULL) {
        const char* curCmd = &info->fmt[0];
        const RegisterInfo* regInfo = NULL;
        RGBA32 color = COLOR_RGBA32_NONE;
        _Bool separator = FALSE;

        clear_saved_reg_buffer();

        // Whether to print immediates as decimal values rather than hexadecimal.
        _Bool decImmediates = (cs_get_setting_val(CS_OPT_GROUP_PAGE_DISASM, CS_OPT_DISASM_IMM_FMT) == PRINT_NUM_FMT_DEC);

        // Loop through the chars in the 'fmt' member of 'info' and print the insn data accordingly.
        for (size_t cmdIndex = 0; cmdIndex < STRLEN(info->fmt); cmdIndex++) {
            if (unimpl || (*curCmd == CHAR_NULL)) {
                break;
            }

            if (separator) {
                separator = FALSE;
                ADD_STR(", ");
            }

            switch (*curCmd) {
                case CHAR_P_NOP: // NOP.
                    ADD_COLOR(COLOR_RGBA32_CRASH_DISASM_NOP);
                    ADD_STR(info->name);
                    return insn_as_string;
                case CHAR_P_NAME: // Instruction name.
                    ADD_COLOR(COLOR_RGBA32_CRASH_DISASM_INSN);
                    ADD_STR(STR_INSN_NAME, info->name);
                    break;
                case CHAR_P_NAMEF: // Instruction name with format.
                    ADD_COLOR(COLOR_RGBA32_CRASH_DISASM_INSN);
                    bzero(insn_name, sizeof(insn_name));
                    sprintf(insn_name, STR_INSN_NAME_FORMAT, info->name, cop1_fmt_to_char(insn));
                    ADD_STR(STR_INSN_NAME, insn_name);
                    break;
                case CHAR_P_RS: // CPU 'RS' register.
                    ADD_COLOR(COLOR_RGBA32_CRASH_VARIABLE);
                    ADD_REG(STR_IREG, CPU, insn.rs);
                    separator = TRUE;
                    break;
                case CHAR_P_RT: // CPU 'RT' register.
                    ADD_COLOR(COLOR_RGBA32_CRASH_VARIABLE);
                    ADD_REG(STR_IREG, CPU, insn.rt);
                    separator = TRUE;
                    break;
                case CHAR_P_RD: // CPU 'RD' register.
                    ADD_COLOR(COLOR_RGBA32_CRASH_VARIABLE);
                    ADD_REG(STR_IREG, CPU, insn.rd);
                    separator = TRUE;
                    break;
                case CHAR_P_IMM: // Immediate.
                    ADD_COLOR(COLOR_RGBA32_CRASH_DISASM_IMMEDIATE);
                    ADD_STR((decImmediates ? "%d" : STR_IMMEDIATE), insn.immediate);
                    break;
                case CHAR_P_NIMM: // Negative Immediate.
                    ADD_COLOR(COLOR_RGBA32_CRASH_DISASM_IMMEDIATE);
                    ADD_STR((decImmediates ? "%d" : STR_IMMEDIATE), -(s16)insn.immediate);
                    break;
                case CHAR_P_SHIFT: // Shift amount.
                    ADD_COLOR(COLOR_RGBA32_CRASH_DISASM_IMMEDIATE);
                    ADD_STR(STR_IMMEDIATE, insn.sa);
                    break;
                case CHAR_P_BASE: // Register offset base.
                    ADD_COLOR(COLOR_RGBA32_CRASH_VARIABLE);
                    ADD_REG(STR_IREG_BASE, CPU, insn.base);
                    break;
                case CHAR_P_BRANCH: // Branch offset.
                    ADD_COLOR(COLOR_RGBA32_CRASH_OFFSET);
                    if (cs_get_setting_val(CS_OPT_GROUP_PAGE_DISASM, CS_OPT_DISASM_OFFSET_ADDR)) {
                        ADD_STR(STR_FUNCTION, get_insn_branch_target_from_addr(addr));
                    } else {
                        s16 branchOffset = (insn.offset + 1);
                        ADD_STR(STR_OFFSET, ((branchOffset < 0x0000) ? '-' : '+'), abss(branchOffset)); //! TODO: Use '%+' format specifier if possible with 0x prefix.
                    }
                    break;
                case CHAR_P_COP0D: // COP0 'RD' register.
                    ADD_COLOR(COLOR_RGBA32_CRASH_VARIABLE);
                    ADD_REG(STR_IREG, COP0, insn.rd);
                    separator = TRUE;
                    break;
                case CHAR_P_FT: // COP1 'FT' register.
                    ADD_COLOR(COLOR_RGBA32_CRASH_VARIABLE);
                    ADD_REG(STR_IREG, COP1, insn.ft);
                    separator = TRUE;
                    break;
                case CHAR_P_FS: // COP1 'FS' register.
                    ADD_COLOR(COLOR_RGBA32_CRASH_VARIABLE);
                    ADD_REG(STR_IREG, COP1, insn.fs);
                    separator = TRUE;
                    break;
                case CHAR_P_FD: // COP1 'FD' register.
                    ADD_COLOR(COLOR_RGBA32_CRASH_VARIABLE);
                    ADD_REG(STR_IREG, COP1, insn.fd);
                    separator = TRUE;
                    break;
                case CHAR_P_FUNC: // Jump function.
                    ADD_COLOR(COLOR_RGBA32_CRASH_FUNCTION_NAME);
                    Address target = PHYSICAL_TO_VIRTUAL(insn.instr_index * sizeof(InsnData));
#ifdef INCLUDE_DEBUG_MAP
                    if (cs_get_setting_val(CS_OPT_GROUP_GLOBAL, CS_OPT_GLOBAL_SYMBOL_NAMES) && is_in_code_segment(target)) {
                        const MapSymbol* symbol = get_map_symbol(target, SYMBOL_SEARCH_BACKWARD);
                        if (symbol != NULL) {
                            *fname = get_map_symbol_name(symbol);
                            // Only print as the function name if it's the exact starting address of the function.
                            if (target != symbol->addr) {
                                *fname = NULL;
                            }

                            if (*fname != NULL) {
                                break;
                            }
                        }
                    }
#else // !INCLUDE_DEBUG_MAP
                    *fname = NULL;
#endif // !INCLUDE_DEBUG_MAP
                    ADD_STR(STR_FUNCTION, target);
                    break;
                default: // Unknown parameter.
                    unimpl = TRUE;
                    break;
            }

            curCmd++;
        }
    } else {
        unimpl = TRUE;
    }

    if (unimpl) { //! TODO: binary mode for these.
        ADD_STR(("unimpl "STR_HEX_WORD), insn.raw);
    }

    return insn_as_string;
}