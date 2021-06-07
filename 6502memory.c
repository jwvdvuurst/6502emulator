#include <stdbool.h>
#include <stdio.h>

#include "6502memory.h"

BYTE memory[0xFFFF] = {0x00};

BYTE fetch_byte(WORD address) {
    return memory[address];
}

void store_byte(WORD address, BYTE data) {
    memory[address] = data;
}

WORD fetch_word(WORD address) {
    return memory[address] + (memory[address+1] << 8);
}

void store_word(WORD address, WORD data) {
    memory[address]   = (BYTE)(data & 0x00FF);
    memory[address+1] = (BYTE)((data & 0xFF00) >> 8);
}

bool memory_test() {
    bool ok = true;

    WORD values[8] = {0x0123, 0x4567, 0x89AB, 0xCDEF, 0x1234, 0x5678, 0x9ABC, 0xDEF0};

    for( WORD address=0x0000; address < 0xFFFF; address++ ) {
        store_byte(address, 0xFF);
        ok = ok && (fetch_byte(address) == 0xFF);

        if (!ok) {
            fprintf(stderr, "memory_test: error with byte test (value 0xFF) on address %d\n", address );
            return false;
        }

        WORD value = values[address%8];
        WORD retval = 0x0000;
        store_word(address, value);
        retval = fetch_word(address);

        ok = ok && (retval == value);
        if (!ok) {
            fprintf(stderr, "memory_test: error with word test (value %d, returned as %d) on address %d\n", value, retval, address );
            return false;
        }
    }

    for( WORD address=0x0000; address < 0xFFFF; address++ ) {
        memory[address] = 0xEA;
    }

    return true;
}

