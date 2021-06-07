#ifndef CPU_6502IO_H
#define CPU_6502IO_H

#include "6502types.h"

bool clear_display();

bool write_string(BYTE x, BYTE y,const char* text);
bool scroll(BYTE l);
bool write_statmessage(const char* message);

bool display();
bool init_io();
bool close_io();

bool backspace_pressed();

bool display_test();

#endif // CPU_6502IO_H
