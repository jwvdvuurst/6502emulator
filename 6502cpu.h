#ifndef CPU_6502CPU_H
#define CPU_6502CPU_H

#include "6502types.h" 

BYTE fetch();
bool execute( BYTE instruction );
bool tick();
bool cpu_test();

typedef struct cpu_t {
	BYTE A;
	BYTE X;
	BYTE Y;
	BYTE SP;
	BYTE P;
	WORD PC;
	BYTE current_instruction;
	BYTE *stack;
	unsigned long long cycles;
} cpu_t;

bool cpu_init();
unsigned long long cpu_get_cycles();

#endif //CPU_6502CPU_H
