#include <stdbool.h>
#include <stdio.h>

#include "6502memory.h"

BYTE memory[0x10000] = {0x00};

BYTE fetch_byte(WORD address) {
    return memory[address & 0xFFFF];
}

void store_byte(WORD address, BYTE data) {
    memory[address & 0xFFFF] = data;
}

WORD fetch_word(WORD address) {
    BYTE lo = memory[address & 0xFFFF];
    BYTE hi = memory[(address + 1) & 0xFFFF];
    return (WORD)(lo | (hi << 8));
}

void store_word(WORD address, WORD data) {
    memory[address & 0xFFFF]       = (BYTE)(data & 0x00FF);
    memory[(address + 1) & 0xFFFF] = (BYTE)((data & 0xFF00) >> 8);
}

bool memory_test() {
    bool ok = true;

    WORD values[8] = {0x0123, 0x4567, 0x89AB, 0xCDEF, 0x1234, 0x5678, 0x9ABC, 0xDEF0};

    for( WORD address=0x0000; address < 0x10000; address++ ) {
        store_byte(address, 0xFF);
        ok = ok && (fetch_byte(address) == 0xFF);

        if (!ok) {
            fprintf(stderr, "memory_test: error with byte test (value 0xFF) on address %u\n", address );
            return false;
        }

        WORD value = values[address%8];
        WORD retval = 0x0000;
        store_word(address, value);
        retval = fetch_word(address);

        ok = ok && (retval == value);
        if (!ok) {
            fprintf(stderr, "memory_test: error with word test (value %u, returned as %u) on address %u\n", value, retval, address );
            return false;
        }
    }

    for( WORD address=0x0000; address < 0x10000; address++ ) {
        memory[address] = 0xEA;
    }

    return true;
}

