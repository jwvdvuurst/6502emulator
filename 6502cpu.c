#include <stdio.h>
#include <stdbool.h>

#include "6502memory.h"
#include "6502types.h"
#include "6502io.h"

BYTE  stackpointer = 0x00;
BYTE* stack = NULL;
BYTE  accumulator;
BYTE  regx;
BYTE  regy;
BYTE  procstatus;
WORD  programcounter = 0xFFFE;

BYTE  current_instruction = {0x00};

static bool initialised = false;
char stat_message[80];

#define CARRY_BIT 0
#define ZERO_BIT 1
#define INTERRUPT_BIT 2
#define DECIMAL_BIT 3
#define BREAK_BIT 4
#define OVERFLOW_BIT 6
#define NEGATIVE_BIT 7

#define CARRY_FLAG      ((procstatus & (0x01 << CARRY_BIT)) != 0x00)
#define ZERO_FLAG       ((procstatus & (0x01 << ZERO_BIT)) != 0x00)
#define INTERRUPT_FLAG  ((procstatus & (0x01 << INTERRUPT_BIT)) != 0x00)
#define DECIMAL_FLAG    ((procstatus & (0x01 << DECIMAL_BIT)) != 0x00)
#define BREAK_FLAG      ((procstatus & (0x01 << BREAK_BIT)) != 0x00)
#define OVERFLOW_FLAG   ((procstatus & (0x01 << OVERFLOW_BIT)) != 0x00)
#define NEGATIVE_FLAG   ((procstatus & (0x01 << NEGATIVE_BIT)) != 0x00)

#define CARRY_SET      procstatus = (procstatus | (0x01 << CARRY_BIT))
#define ZERO_SET       procstatus = (procstatus | (0x01 << ZERO_BIT))
#define INTERRUPT_SET  procstatus = (procstatus | (0x01 << INTERRUPT_BIT))
#define DECIMAL_SET    procstatus = (procstatus | (0x01 << DECIMAL_BIT))
#define BREAK_SET      procstatus = (procstatus | (0x01 << BREAK_BIT))
#define OVERFLOW_SET   procstatus = (procstatus | (0x01 << OVERFLOW_BIT))
#define NEGATIVE_SET   procstatus = (procstatus | (0x01 << NEGATIVE_BIT))

#define CARRY_CLEAR      procstatus = (procstatus ^ (0x01 << CARRY_BIT))
#define ZERO_CLEAR       procstatus = (procstatus ^ (0x01 << ZERO_BIT))
#define INTERRUPT_CLEAR  procstatus = (procstatus ^ (0x01 << INTERRUPT_BIT))
#define DECIMAL_CLEAR    procstatus = (procstatus ^ (0x01 << DECIMAL_BIT))
#define BREAK_CLEAR      procstatus = (procstatus ^ (0x01 << BREAK_BIT))
#define OVERFLOW_CLEAR   procstatus = (procstatus ^ (0x01 << OVERFLOW_BIT))
#define NEGATIVE_CLEAR   procstatus = (procstatus ^ (0x01 << NEGATIVE_BIT))

#define NOTI             fprintf(stderr,"Function %s is not implemented\n",__func__);
#define FETCH_ADDRESS    switch(mode) { \
                            case INMEDIATE: address = fetch_word(programcounter); \
                                            programcounter += 2; \
                                            break; \
                            case ZEROPAGE:  address = 0x0000 + fetch_byte(programcounter++); \
                                            break; \
                            case ABSOLUTE:  value = fetch_byte(programcounter++); \
                                            break; \
                            case INDIRECT:  address = fetch_word(programcounter); \
                                            programcounter += 2; \
                                            address = fetch_word(address); \
                                            break; \
                            case NOMODE:    break; \
                            default:        NOTI; \
                                            break; \
                         }
#define STATUS           sprintf(stat_message, "OC %s: IN: %02X pc: %04X am: %s\tsp: %02X ps: %02X acc: %02X rx:%02X ry: %02X", \ 
                                               __func__, current_instruction, programcounter, printMode(mode), \
                                               stackpointer, procstatus, accumulator, regx, regy); \
                         write_statmessage(stat_message);

#define EXECUTE_OPCODE(actgen, actin, actzp, actab, actid, actno)   STATUS \
                                                            bool ok = true; \
                                                            WORD address = {0x0000}; \
                                                            BYTE value = {0x00}; \
                                                            FETCH_ADDRESS \
                                                            actgen \
                                                            switch(mode) { \
                                                                case INMEDIATE: actin; break; \
                                                                case ZEROPAGE:  actzp; break; \
                                                                case ABSOLUTE:  actab; break; \
                                                                case INDIRECT:  actid; break; \
                                                                case NOMODE:    actno; break; \
                                                            } \
                                                            return ok;
                                                                            
#define PUSH(a) stack != NULL ? stackpointer < 0xFF ? (stack[stackpointer++] = a, true) : false : false
#define POP(a)  stack != NULL ? stackpointer > 0x00 ? (a = stack[stackpointer--], true) : false : false

#define IMP NOMODE, NONE
#define IMM INMEDIATE, NONE
#define ZP  ZEROPAGE, NONE
#define ZPX ZEROPAGE, REGX
#define ZPY ZEROPAGE, REGY
#define IZX INDIRECT, REGX
#define IZY INDIRECT, REGY
#define ABS ABSOLUTE, NONE
#define ABX ABSOLUTE, REGX
#define ABY ABSOLUTE, REGY
#define IND INDIRECT, NONE
#define REL NOMODE, NONE   //not implemented yet

#define OPCALL(opcde, mde)  ok = ok && opcde(mde); break;

typedef enum {
    INMEDIATE,
    ZEROPAGE,
    ABSOLUTE,
    INDIRECT,
    NOMODE
} mode_t;

typedef enum {
    NONE,
    REGX,
    REGY
} register_t;

static bool wpush( WORD a ) {
    if (stack == NULL) return false;
    if (stackpointer >= 0xFE) return false;
    PUSH((BYTE)(a & 0x00FF));
    PUSH((BYTE)((a & 0xFF00)>>8));
    return true;
}

static bool wpop( WORD* a ) {
    if (stack == NULL) return false;
    if (stackpointer <= 0x01) return false;
    BYTE hi = {0x00};
    BYTE lo = {0x00};
    POP(hi);
    POP(lo);
    *a = (WORD)((hi<<8)+lo);
    return true;
} 

static const char* printMode(mode_t mode) {
    switch(mode) {
        case INMEDIATE: return "INMEDIATE"; break;
        case ZEROPAGE:  return "ZEROPAGE"; break;
        case ABSOLUTE:  return "ABSOLUTE"; break;
        case INDIRECT:  return "INDIRECT"; break;
        case NOMODE:    return "NOMODE"; break;
        default:        return "**error**"; break;
    }
}

// Add with carry
static inline bool ADC( mode_t mode, register_t reg ) {
    EXECUTE_OPCODE( BYTE addcarry = CARRY_FLAG?1:0; CARRY_CLEAR; , \
                    value = fetch_byte(address); \
                    if (accumulator + value + addcarry < 0xFF) { \ 
                       accumulator += (value + addcarry); \
                    } else { \
                       accumulator = (accumulator + value + addcarry) - 0xFF; \
                       CARRY_SET; \
                    } , \
                    value = fetch_byte(address); \
                    if (accumulator + value + addcarry < 0xFF) { \ 
                       accumulator += (value + addcarry); \
                    } else { \
                       accumulator = (accumulator + value + addcarry) - 0xFF; \
                       CARRY_SET; \
                    } , \
                    if (accumulator + value + addcarry < 0xFF) { \ 
                       accumulator += (value + addcarry); \
                    } else { \
                       accumulator = (accumulator + value + addcarry) - 0xFF; \
                       CARRY_SET; \
                    } , \
                    value = fetch_byte(address); \
                    if (accumulator + value + addcarry < 0xFF) { \ 
                       accumulator += (value + addcarry); \
                    } else { \
                       accumulator = (accumulator + value + addcarry) - 0xFF; \
                       CARRY_SET; \
                    } , \
                    NOTI )
}

// arithmetic shift left
static inline bool ASL( mode_t mode, register_t reg ) {
    EXECUTE_OPCODE(,,,,,);
}
// and (with accumulator)
static inline bool AND( mode_t mode, register_t reg ) {
    EXECUTE_OPCODE(value=fetch_byte(address);accumulator&=value;,,,,,);
}

// branch on carry clear
static inline bool BCC( mode_t mode, register_t reg ) {
    EXECUTE_OPCODE(,,,,,);
}

// branch on carry set
static inline bool BCS( mode_t mode, register_t reg ) {
    EXECUTE_OPCODE(,,,,,);
}

// branch on equal (zero set)
static inline bool BEQ( mode_t mode, register_t reg ) {
    EXECUTE_OPCODE(,,,,,);
}

// bit test
static inline bool BIT( mode_t mode, register_t reg ) {
    EXECUTE_OPCODE(,,,,,);
}

// branch on minus (negative set)
static inline bool BMI( mode_t mode, register_t reg ) {
    EXECUTE_OPCODE(if(NEGATIVE_SET) programcounter=address;,,,,,);
}

// branch on not equal (zero clear)
static inline bool BNE( mode_t mode, register_t reg ) {
    EXECUTE_OPCODE(,,,,,);
}

// branch on plus (negative clear)
static inline bool BPL( mode_t mode, register_t reg ) {
    EXECUTE_OPCODE(if(NEGATIVE_SET) {;} else { programcounter=address; },,,,,);
}

// break / interrupt
static inline bool BRK( mode_t mode, register_t reg ) {
    EXECUTE_OPCODE(,,,,,);
}

// branch on overflow clear
static inline bool BVC( mode_t mode, register_t reg ) {
    EXECUTE_OPCODE(,,,,,);
}

// branch on overflow set
static inline bool BVS( mode_t mode, register_t reg ) {
    EXECUTE_OPCODE(,,,,,);
}

// clear carry
static inline bool CLC( mode_t mode, register_t reg ) {
   EXECUTE_OPCODE(CARRY_CLEAR;,,,,,);
}

// clear decimal
static inline bool CLD( mode_t mode, register_t reg ) {
   EXECUTE_OPCODE(DECIMAL_CLEAR;,,,,,);
}

// clear interrupt disable
static inline bool CLI( mode_t mode, register_t reg ) {
    EXECUTE_OPCODE(INTERRUPT_CLEAR;,,,,,);
}

// clear overflow
static inline bool CLV( mode_t mode, register_t reg ) {
    EXECUTE_OPCODE(OVERFLOW_CLEAR;,,,,,);
}

// compare (with accumulator)
static inline bool CMP( mode_t mode, register_t reg ) {
    EXECUTE_OPCODE(,,,,,);
}

// compare with X
static inline bool CPX( mode_t mode, register_t reg ) {
    EXECUTE_OPCODE(,,,,,);
}


// compare with Y
static inline bool CPY( mode_t mode, register_t reg ) {
    EXECUTE_OPCODE(,,,,,);
}

// decrement
static inline bool DEC( mode_t mode, register_t reg ) {
    EXECUTE_OPCODE(accumulator--;,,,,,);
}

// decrement X
static inline bool DEX( mode_t mode, register_t reg ) {
    EXECUTE_OPCODE(regx--;,,,,,);
}

// decrement Y
static inline bool DEY( mode_t mode, register_t reg ) {
    EXECUTE_OPCODE(regy--;,,,,,);
}

// exclusive or (with accumulator)
static inline bool EOR( mode_t mode, register_t reg ) {
    EXECUTE_OPCODE(value=fetch_byte(address);accumulator^=value;,,,,,);
}

// increment
static inline bool INC( mode_t mode, register_t reg ) {
    EXECUTE_OPCODE(accumulator++;,,,,,);
}

// increment X
static inline bool INX( mode_t mode, register_t reg ) {
    EXECUTE_OPCODE(regx++;,,,,,);
}

// increment Y
static inline bool INY( mode_t mode, register_t reg ) {
    EXECUTE_OPCODE(regy++;,,,,,);
}

// jump
static inline bool JMP( mode_t mode, register_t reg ) {
    EXECUTE_OPCODE( programcounter = address;, , , , , );
}

// jump subroutine
static inline bool JSR( mode_t mode, register_t reg ) {
    EXECUTE_OPCODE( wpush(programcounter); programcounter = address;, , , , , );
}

// load accumulator
static inline bool LDA( mode_t mode, register_t reg ) {
    EXECUTE_OPCODE(value=fetch_byte(address);accumulator=value;,,,,,);
}

// load X
static inline bool LDX( mode_t mode, register_t reg ) {
    EXECUTE_OPCODE(value=fetch_byte(address);regx=value;,,,,,);
}

// load Y
static inline bool LDY( mode_t mode, register_t reg ) {
    EXECUTE_OPCODE(value=fetch_byte(address);regy=value;,,,,,);
}

// logical shift right
static inline bool LSR( mode_t mode, register_t reg ) {
    EXECUTE_OPCODE(,,,,,);
}

//  no operation
static inline bool NOP( mode_t mode, register_t reg ) {
    EXECUTE_OPCODE(,,,,,);
}

// or with accumulator
static inline bool ORA( mode_t mode, register_t reg ) {
    EXECUTE_OPCODE(value=fetch_byte(address);accumulator|=value;,,,,,);
}

// push accumulator
static inline bool PHA( mode_t mode, register_t reg ) {
    EXECUTE_OPCODE(PUSH(accumulator);,,,,,);
} 

// push processor status (SR)
static inline bool PHP( mode_t mode, register_t reg ) {
    EXECUTE_OPCODE(PUSH(procstatus);,,,,,);
}

// pull accumulator
static inline bool PLA( mode_t mode, register_t reg ) {
    EXECUTE_OPCODE(POP(accumulator);,,,,,);
}

// pull processor status (SR)
static inline bool PLP( mode_t mode, register_t reg ) {
    EXECUTE_OPCODE(POP(procstatus);,,,,,);
}

// rotate left
static inline bool ROL( mode_t mode, register_t reg ) {
    EXECUTE_OPCODE(BYTE bit=(accumulator & 0x80)>>7;accumulator=(accumulator<<1)+bit;,,,,,);
}

// rotate right
static inline bool ROR( mode_t mode, register_t reg ) {
    EXECUTE_OPCODE(BYTE bit=(accumulator & 0x01)<<7;accumulator=(accumulator>>1)+bit;,,,,,);
}

// return from interrupt
static inline bool RTI( mode_t mode, register_t reg ) {
    EXECUTE_OPCODE( wpop(&programcounter);, , , , , );
}

// return from subroutine
static inline bool RTS( mode_t mode, register_t reg ) {
    EXECUTE_OPCODE( wpop(&programcounter);, , , , , );
}

// subtract with carry
static inline bool SBC( mode_t mode, register_t reg ) {
    EXECUTE_OPCODE(,,,,,);
}

// set carry
static inline bool SEC( mode_t mode, register_t reg ) {
    EXECUTE_OPCODE(CARRY_SET;,,,,,);
}

// set decimal
static inline bool SED( mode_t mode, register_t reg ) {
    EXECUTE_OPCODE(DECIMAL_SET;,,,,,);
}

// set interrupt disable
static inline bool SEI( mode_t mode, register_t reg ) {
    EXECUTE_OPCODE(,,,,,);
}

// store accumulator
static inline bool STA( mode_t mode, register_t reg ) {
    EXECUTE_OPCODE(store_byte(address,accumulator);,,,,,);
}

// store X
static inline bool STX( mode_t mode, register_t reg ) {
    EXECUTE_OPCODE(store_byte(address,regx);,,,,,);
}

// store Y
static inline bool STY( mode_t mode, register_t reg ) {
    EXECUTE_OPCODE(store_byte(address,regy);,,,,,);
}

// transfer accumulator to X
static inline bool TAX( mode_t mode, register_t reg ) {
    EXECUTE_OPCODE(regx=accumulator;,,,,,);
}

// transfer accumulator to Y
static inline bool TAY( mode_t mode, register_t reg ) {
    EXECUTE_OPCODE(regy=accumulator;,,,,,);
    return true;
}

// transfer stack pointer to X
static inline bool TSX( mode_t mode, register_t reg ) {
    EXECUTE_OPCODE(regx=stackpointer;,,,,,);
}

// transfer X to accumulator
static inline bool TXA( mode_t mode, register_t reg ) {
    EXECUTE_OPCODE(accumulator=regx;,,,,,);
}

// transfer Y to stack pointer
static inline bool TXS( mode_t mode, register_t reg ) {
    EXECUTE_OPCODE(stackpointer=regx;,,,,,);
}

// transfer Y to accumulator
static inline bool TYA( mode_t mode, register_t reg ) {
    EXECUTE_OPCODE(accumulator=regy;,,,,,);
}



/*------------------------------------------------------------------------------------------------------------------------------
 * HI	LO-NIBBLE
 *      ‐0	        ‐1	        ‐2	‐3	‐4	‐5	    ‐6	    ‐7	‐8	        ‐9	    ‐A	    ‐B	‐C	‐D	    ‐E	    ‐F
 * 0‐	BRK impl	ORA X,ind	---	---	---	ORA zpg	ASL zpg	---	PHP impl	ORA #	ASL A	---	---	ORA abs	ASL abs	---
 * 1‐	BPL rel	ORA ind,Y	---	---	---	ORA zpg,X	ASL zpg,X	---	CLC impl	ORA abs,Y	---	---	---	ORA abs,X	ASL abs,X	---
 * 2‐	JSR abs	AND X,ind	---	---	BIT zpg	AND zpg	ROL zpg	---	PLP impl	AND #	ROL A	---	BIT abs	AND abs	ROL abs	---
 * 3‐	BMI rel	AND ind,Y	---	---	---	AND zpg,X	ROL zpg,X	---	SEC impl	AND abs,Y	---	---	---	AND abs,X	ROL abs,X	---
 * 4‐	RTI impl	EOR X,ind	---	---	---	EOR zpg	LSR zpg	---	PHA impl	EOR #	LSR A	---	JMP abs	EOR abs	LSR abs	---
 * 5‐	BVC rel	EOR ind,Y	---	---	---	EOR zpg,X	LSR zpg,X	---	CLI impl	EOR abs,Y	---	---	---	EOR abs,X	LSR abs,X	---
 * 6‐	RTS impl	ADC X,ind	---	---	---	ADC zpg	ROR zpg	---	PLA impl	ADC #	ROR A	---	JMP ind	ADC abs	ROR abs	---
 * 7‐	BVS rel	ADC ind,Y	---	---	---	ADC zpg,X	ROR zpg,X	---	SEI impl	ADC abs,Y	---	---	---	ADC abs,X	ROR abs,X	---
 * 8‐	---	STA X,ind	---	---	STY zpg	STA zpg	STX zpg	---	DEY impl	---	TXA impl	---	STY abs	STA abs	STX abs	---
 * 9‐	BCC rel	STA ind,Y	---	---	STY zpg,X	STA zpg,X	STX zpg,Y	---	TYA impl	STA abs,Y	TXS impl	---	---	STA abs,X	---	---
 * A‐	LDY #	LDA X,ind	LDX #	---	LDY zpg	LDA zpg	LDX zpg	---	TAY impl	LDA #	TAX impl	---	LDY abs	LDA abs	LDX abs	---
 * B‐	BCS rel	LDA ind,Y	---	---	LDY zpg,X	LDA zpg,X	LDX zpg,Y	---	CLV impl	LDA abs,Y	TSX impl	---	LDY abs,X	LDA abs,X	LDX abs,Y	---
 * C‐	CPY #	CMP X,ind	---	---	CPY zpg	CMP zpg	DEC zpg	---	INY impl	CMP #	DEX impl	---	CPY abs	CMP abs	DEC abs	---
 * D‐	BNE rel	CMP ind,Y	---	---	---	CMP zpg,X	DEC zpg,X	---	CLD impl	CMP abs,Y	---	---	---	CMP abs,X	DEC abs,X	---
 * E‐	CPX #	SBC X,ind	---	---	CPX zpg	SBC zpg	INC zpg	---	INX impl	SBC #	NOP impl	---	CPX abs	SBC abs	INC abs	---
 * F‐	BEQ rel	SBC ind,Y	---	---	---	SBC zpg,X	INC zpg,X	---	SED impl	SBC abs,Y	---	---	---	SBC abs,X	INC abs,X	---
 */
BYTE fetch() {
    return fetch_byte(programcounter++);
}
bool execute(BYTE instruction) {
    bool ok = true;
    WORD address = {0x00};

    current_instruction = instruction;

    switch(instruction) {
        case 0x00: OPCALL(BRK,IMP);
        case 0x01: OPCALL(ORA,IZX);
        case 0x02: break; //reserved
        case 0x03: break; //reserved
        case 0x04: break; //reserved 
        case 0x05: OPCALL(ORA,ZP);
        case 0x06: OPCALL(ASL,ZP);
        case 0x07: break;
        case 0x08: OPCALL(PHP,IMP);
        case 0x09: OPCALL(ORA,IMM);
        case 0x0A: OPCALL(ASL,IMM);
        case 0x0B: break;
        case 0x0C: break;
        case 0x0D: break;
        case 0x0E: break;
        case 0x0F: break;
        case 0x10: OPCALL(BPL,IMM);
        case 0x11: OPCALL(ORA,IZY); 
        case 0x12: break;
        case 0x13: break;
        case 0x14: break;
        case 0x15: OPCALL(ORA,ZPX);
        case 0x16: break;
        case 0x17: break;
        case 0x18: OPCALL(CLC,IMM);
        case 0x19: break;
        case 0x1A: break;
        case 0x1B: break;
        case 0x1C: break;
        case 0x1D: break;
        case 0x1E: break;
        case 0x1F: break;
        case 0x20: OPCALL(JSR,ABS);
        case 0x21: OPCALL(AND,IZX);
        case 0x22: break;
        case 0x23: break;
        case 0x24: OPCALL(BIT,ZP);
        case 0x25: OPCALL(AND,ZP);
        case 0x26: break;
        case 0x27: break;
        case 0x28: OPCALL(PLP,IMP);
        case 0x29: OPCALL(AND,IMM);
        case 0x2A: break;
        case 0x2B: break;
        case 0x2C: OPCALL(BIT,ABS);
        case 0x2D: OPCALL(AND,ABS);
        case 0x2E: break;
        case 0x2F: break;
        case 0x30: OPCALL(BMI,IMM);
        case 0x31: OPCALL(AND,IZY);
        case 0x32: break;
        case 0x33: break;
        case 0x34: break;
        case 0x35: OPCALL(AND,ZPX);
        case 0x36: break;
        case 0x37: break;
        case 0x38: OPCALL(SEC,ABS);
        case 0x39: OPCALL(AND,ABY);
        case 0x3A: break;
        case 0x3B: break;
        case 0x3C: break;
        case 0x3D: OPCALL(AND,ABX);
        case 0x3E: break;
        case 0x3F: break;
        case 0x40: OPCALL(RTI,IMP);
        case 0x41: OPCALL(EOR,IZX);
        case 0x42: break;
        case 0x43: break;
        case 0x44: break;
        case 0x45: OPCALL(EOR,ZP);
        case 0x46: break;
        case 0x47: break;
        case 0x48: OPCALL(PHA,IMP);
        case 0x49: OPCALL(EOR,IMM); 
        case 0x4A: break;
        case 0x4B: break;
        case 0x4C: OPCALL(JMP,ABS);
        case 0x4D: OPCALL(EOR,ABS); 
        case 0x4E: break;
        case 0x4F: break;
        case 0x50: OPCALL(BVC,IMM);
        case 0x51: OPCALL(EOR,IZY);
        case 0x52: break;
        case 0x53: break;
        case 0x54: break;
        case 0x55: OPCALL(EOR,ZPX);
        case 0x56: break;
        case 0x57: break;
        case 0x58: OPCALL(CLI,IMP);
        case 0x59: OPCALL(EOR,ABY);
        case 0x5A: break;
        case 0x5B: break;
        case 0x5C: break;
        case 0x5D: OPCALL(EOR,ABX);
        case 0x5E: break;
        case 0x5F: break;
        case 0x60: OPCALL(RTS,IMP);
        case 0x61: OPCALL(ADC,IZX);
        case 0x62: break;
        case 0x63: break;
        case 0x64: break;
        case 0x65: OPCALL(ADC,ZP);
        case 0x66: break;
        case 0x67: break;
        case 0x68: break;
        case 0x69: OPCALL(ADC,IMM);
        case 0x6A: break;
        case 0x6B: OPCALL(PLA,IMP);
        case 0x6C: OPCALL(JMP,IND);
        case 0x6D: OPCALL(ADC,ABS);
        case 0x6E: break;
        case 0x6F: break;
        case 0x70: OPCALL(BVS,IMM);
        case 0x71: OPCALL(ADC,IZY);
        case 0x72: break;
        case 0x73: break;
        case 0x74: break;
        case 0x75: OPCALL(ADC,ZPX);
        case 0x76: break;
        case 0x77: break;
        case 0x78: OPCALL(SEI,IMP);
        case 0x79: OPCALL(ADC,ABY);
        case 0x7A: break;
        case 0x7B: break;
        case 0x7C: break;
        case 0x7D: OPCALL(ADC,ABX);
        case 0x7E: break;
        case 0x7F: break;
        case 0x80: break;
        case 0x81: OPCALL(STA,IZX);
        case 0x82: break;
        case 0x83: break;
        case 0x84: OPCALL(STY,ZP);
        case 0x85: OPCALL(STA,ZP);
        case 0x86: OPCALL(STX,ZP);
        case 0x87: break;
        case 0x88: OPCALL(DEY,IMP);
        case 0x89: break;
        case 0x8A: OPCALL(TXA,IMP);
        case 0x8B: break;
        case 0x8C: OPCALL(STY,ABS);
        case 0x8D: break;
        case 0x8E: OPCALL(STX,ABS);
        case 0x8F: break;
        case 0x90: OPCALL(BCC,IMM);
        case 0x91: OPCALL(STA,IZY);
        case 0x92: break;
        case 0x93: break;
        case 0x94: OPCALL(STY,ZPX);
        case 0x95: OPCALL(STA,ZPX);
        case 0x96: OPCALL(STX,ZPY);
        case 0x97: break;
        case 0x98: OPCALL(TYA,IMP);
        case 0x99: OPCALL(STA,ABY);
        case 0x9A: OPCALL(TXS,IMP);
        case 0x9B: break;
        case 0x9C: break;
        case 0x9D: break;
        case 0x9E: break;
        case 0x9F: break;
        case 0xA0: OPCALL(LDY,IMM);
        case 0xA1: OPCALL(LDA,IZX);
        case 0xA2: OPCALL(LDX,IMM);
        case 0xA3: break;
        case 0xA4: OPCALL(LDY, ZP);
        case 0xA5: OPCALL(LDA, ZP);
        case 0xA6: OPCALL(LDX, ZP);
        case 0xA7: break;
        case 0xA8: break;
        case 0xA9: OPCALL(LDA,IMM);
        case 0xAA: OPCALL(TAX, IMP);
        case 0xAB: OPCALL(TAY, IMP);
        case 0xAC: OPCALL(LDY,ABS);
        case 0xAD: OPCALL(LDA,ABS);
        case 0xAE: OPCALL(LDX,ABS);
        case 0xAF: break;
        case 0xB0: OPCALL(BCS,IMM);
        case 0xB1: OPCALL(LDA,IZY); 
        case 0xB2: break;
        case 0xB3: break;
        case 0xB4: OPCALL(LDY, ZPX);
        case 0xB5: OPCALL(LDA, ZPX);
        case 0xB6: OPCALL(LDX, ZPY);
        case 0xB7: break;
        case 0xB8: OPCALL(CLV, IMP);
        case 0xB9: OPCALL(LDA,ABY);
        case 0xBA: OPCALL(TSX, IMP);
        case 0xBB: break;
        case 0xBC: OPCALL(LDY,ABX);
        case 0xBD: OPCALL(LDA,ABX);
        case 0xBE: OPCALL(LDX,ABY);
        case 0xBF: break;
        case 0xC0: break;
        case 0xC1: break;
        case 0xC2: break;
        case 0xC3: break;
        case 0xC4: break;
        case 0xC5: break;
        case 0xC6: break;
        case 0xC7: break;
        case 0xC8: OPCALL(INY,IMP);
        case 0xC9: break;
        case 0xCA: OPCALL(DEX,IMP);
        case 0xCB: break;
        case 0xCC: break;
        case 0xCD: break;
        case 0xCE: break;
        case 0xCF: break;
        case 0xD0: OPCALL(BNE,IMM);
        case 0xD1: break;
        case 0xD2: break;
        case 0xD3: break;
        case 0xD4: break;
        case 0xD5: break;
        case 0xD6: break;
        case 0xD7: break;
        case 0xD8: OPCALL(CLD,IMM);
        case 0xD9: break;
        case 0xDA: break;
        case 0xDB: break;
        case 0xDC: break;
        case 0xDD: break;
        case 0xDE: break;
        case 0xDF: break;
        case 0xE0: break;
        case 0xE1: OPCALL(SBC,IZX);
        case 0xE2: break;
        case 0xE3: break;
        case 0xE4: break;
        case 0xE5: OPCALL(SBC,ZP);
        case 0xE6: break;
        case 0xE7: break;
        case 0xE8: OPCALL(INX,IMP);
        case 0xE9: OPCALL(SBC,IMM);
        case 0xEA: OPCALL(NOP,IMP);
        case 0xEB: break;
        case 0xEC: break;
        case 0xED: OPCALL(SBC,ABS);
        case 0xEE: break;
        case 0xEF: break;
        case 0xF0: OPCALL(BEQ,IMM);
        case 0xF1: OPCALL(SBC,IZY);
        case 0xF2: break;
        case 0xF3: break;
        case 0xF4: break;
        case 0xF5: OPCALL(SBC,ZPX);
        case 0xF6: break;
        case 0xF7: break;
        case 0xF8: OPCALL(SED,IMM);
        case 0xF9: OPCALL(SBC,ABY);
        case 0xFA: break;
        case 0xFB: break;
        case 0xFC: break;
        case 0xFD: OPCALL(SBC,ABX);
        case 0xFE: break;
        case 0xFF: break;
    }
    return ok;
}

bool cpu_init() {
    if (!initialised) { 
        stack = memory + 0x0100;
        programcounter = fetch_word(0xFFFE);
        initialised = true;
    }
    return initialised;
}

bool tick() {
    return execute( fetch() );
}

bool cpu_test() {
    bool ok = true;
    BYTE val = 0x00;

    int i = 0;

    if (!cpu_init()) {
        printf("Initialisation failed\n");
        return false;
    }

    while( PUSH(val) && (i<300) ) val=(2*val + i++) % 0xFF;

    ok = ok && (i == 0xFF);

    while( POP(val) && (i>0) ) i--;

    ok = ok && (i == 0x00);
    
    if (!ok) {
        printf( "basic CPU stack tests failed\n" );
        return ok;
    } 

    while( i < 0xFF ) {
        val = (BYTE)(i & 0xFF);
        PUSH(val);
        i++;
    };

    while( POP(val) && (ok = (val == i))) i--;

    if (!ok) {
        printf( "advanced CPU stack tests failed\n" );
        return ok;
    }

    procstatus = 0x00;

    ok = ok && (!CARRY_FLAG);
    ok = ok && (!ZERO_FLAG);
    ok = ok && (!INTERRUPT_FLAG);
    ok = ok && (!DECIMAL_FLAG);
    ok = ok && (!BREAK_FLAG);
    ok = ok && (!OVERFLOW_FLAG);
    ok = ok && (!NEGATIVE_FLAG);

    if (!ok) {
        printf("basic tests procstatus flags (reset) failed\n" );
        return ok;
    }

    procstatus = 0xFF;

    ok = ok && (CARRY_FLAG);
    ok = ok && (ZERO_FLAG);
    ok = ok && (INTERRUPT_FLAG);
    ok = ok && (DECIMAL_FLAG);
    ok = ok && (BREAK_FLAG);
    ok = ok && (OVERFLOW_FLAG);
    ok = ok && (NEGATIVE_FLAG);

    if (!ok) {
        printf("basic tests procstatus flags (set) failed\n" );
        return ok;
    }

    procstatus = 0x00;

    CARRY_SET;
    ok = ok && (CARRY_FLAG);
    CARRY_CLEAR;
    ok = ok && (!CARRY_FLAG);

    if (!ok) {
        printf("basic tests procstatus CARRY flag (set/reset) failed\n");
        return ok;
    }

    ok = ok && execute(0xF8);  // opcode for SED
    if (!ok) {
        printf("execution of opcode 0xF8 (SED) failed\n");
        return ok;
    }
    ok = ok && (DECIMAL_FLAG);
    if (!ok) {
        printf("Decimal flag was not set\n");
        return ok;
    }
    ok = ok && execute(0xD8); // opcode for CLD
    if (!ok) {
        printf("execution of opcode 0XD8 (CLD) failed\n");        
        return ok;
    }
    ok = ok && (!DECIMAL_FLAG);
    if (!ok) {
        printf("Decimal flag was not reset\n");
        return ok;
    }

    return ok;
}
