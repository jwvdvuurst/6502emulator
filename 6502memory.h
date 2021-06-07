#ifndef CPU_6502MEMORY_H
#define CPU_6502MEMORY_H

#include "6502types.h"

extern BYTE memory[];

BYTE fetch_byte(WORD address);
WORD fetch_word(WORD address);

void store_byte(WORD address, BYTE data);
void store_word(WORD address, WORD data);

bool memory_test();
#endif //CPU_6502MEMORY_H
