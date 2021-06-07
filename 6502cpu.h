#ifndef CPU_6502CPU_H
#define CPU_6502CPU_H

#include "6502types.h" 

BYTE fetch();
bool execute( BYTE instruction );
bool tick();
bool cpu_test();

#endif //CPU_6502CPU_H
