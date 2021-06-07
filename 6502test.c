#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "6502types.h"
#include "6502memory.h"
#include "6502cpu.h"
#include "6502io.h"

bool loop() {
    int input;
    bool ok = true;
    bool do_exit = false;

    while(ok && !do_exit) {
        do_exit = backspace_pressed();

        if (!do_exit) {
            ok = ok && tick();
            ok = ok && display();
        }
    }
}

static bool load_memory() {
    BYTE program[40] = {0x20,0x09,0x06,0x20,0x0c,0x06,0x20,0x12,0x06,0xa2,0x00,0x60,0xe8,0xe0,0x05,0xd0,0xfb,0x60,0x00,0xa9,0x03,0x4c,0x08,0x06,0x00,0x00,0x00,0x8d,0x00,0x02,0xa9,0xc0,0xaa,0xe8,0x69,0xc4,0x00,0xea,0xea,0xea};
    WORD address = 0x0000;

    srand(time(0));

    store_byte(0x0000,0x60);
    store_byte(0x0001,0x6C);
    store_word(0x0002,0xD000);
    store_byte(0x0004,0x50);
    store_word(0x0005,0x0400);
    store_byte(0x0007,0x70);
    store_word(0x0008,0xE000);
    store_byte(0x000A,0x90);
    store_word(0x000B,0xF000);
    store_byte(0x000D,0xB0);
    store_word(0x000E,0x0000);

    address = 0x0010;

    while (address < 0xA200) {
        store_byte(address,0xE8);
        if (address%3) store_byte(address++,0x00);
        if (address%5) store_byte(address++,0xAE);
        if (address%7) store_byte(address++,0x6B);
        if (address%11) store_byte(address++,0xC8);
        if (address%13) store_byte(address++,0xE8);
        if (address%17) store_byte(address++,0x4C);
        if (address%23) store_byte(address++,0x50);
        store_byte(address++,0x00);
        store_byte(address++,(BYTE)rand()%256);
    }

    store_byte(0xA200,0x60);
    store_byte(0xA201,0x6C);
    store_word(0xA202,0xD000);
    store_byte(0xA204,0x50);
    store_word(0xA205,0x0400);
    store_byte(0xA207,0x70);
    store_word(0xA208,0xE000);
    store_byte(0xA20A,0x90);
    store_word(0xA20B,0xF000);
    store_byte(0xA20D,0xB0);
    store_word(0xA20E,0x0000);

    address = 0xA210;

    while (address < 0xC000) {
        store_byte(address,0xE8);
        if (address%3) store_byte(address++,0x00);
        if (address%5) store_byte(address++,0xAE);
        if (address%7) store_byte(address++,0x6B);
        if (address%11) store_byte(address++,0xC8);
        if (address%13) store_byte(address++,0xE8);
        if (address%17) store_byte(address++,0x4C);
        if (address%23) store_byte(address++,0x50);
        store_byte(address++,0x00);
        store_byte(address++,(BYTE)rand()%256);
    }



    address = 0xD000;

    while(address < 0xFFFF) {
        store_byte(address,program[address%40]);
        address++;
    }

    return true;
}

int main() {
    bool ok = true;
    bool do_exit = false;

    ok = ok && memory_test();

    if (!ok) {
        fprintf(stderr,"Memory test failed, do_exit");
        do_exit = true;
    }

    ok = ok && cpu_test();

    if (!ok && !do_exit) {
        fprintf(stderr,"CPU test failed, do_exit");
        do_exit = true;
    }

    ok = ok && display_test();
        if (!ok && !do_exit) {
        fprintf(stderr,"Display test failed, do_exit");
        do_exit = true;
    }

    ok = ok && load_memory();

    ok = ok && loop();

    if (!ok && !do_exit) {
        fprintf(stderr,"Error occured during loop");
        do_exit = true;
    }

    close_io();

    if (do_exit) exit(1);

    exit(0);
}
