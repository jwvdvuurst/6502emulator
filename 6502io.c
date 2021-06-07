#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <curses.h>

#include "6502types.h"
#include "6502memory.h"
#include "6502cpu.h"

BYTE* displaymem = NULL;

bool  cursesmode = false;
WINDOW* disp_window = NULL;
WINDOW* stat_window = NULL;


bool clear_display() {
    WORD address=0;
    while ((address < 0x0400) && (displaymem[address++] = 0x20));    
}

bool write_char(BYTE x, BYTE y, const char ch ) {
    WORD charbyte = (y*40)+x;

    if (charbyte > 0x0400) return false;

    displaymem[charbyte] = ch;

    return true;
}

bool write_string(BYTE x, BYTE y,const char* text) {
    WORD startbyte = (y*40)+x;
    BYTE len = strlen(text);

    if (len == 0) return true;

    if (startbyte > 0x0400) return false;

    if (startbyte + len > 0x0400) return false;

    for(BYTE i=0; i<len; i++) displaymem[startbyte + i] = text[i];

    return true;
}

bool scroll_display(BYTE l) {
    if (l>=24) return clear_display();
    while(l>0) {
        for( WORD b=0; b<960;b++ ) displaymem[b] = displaymem[b+40];
        for( WORD b=960; b<1000; b++ ) displaymem[b] = 0x20;
        l--;
    }

    return true;
}

bool write_statmessage(const char* message) {
    bool ok = true;
    if (message == NULL) return false;

    wmove(stat_window, 1, 1);
    wclrtoeol(stat_window);
    mvwprintw(stat_window,1,1,"%s",message);
    wrefresh(stat_window);

    return ok;
}

bool display() {
    if (!cursesmode) {
        initscr();
        cbreak();
        echo();
        keypad(stdscr,true);
        disp_window = newwin(27,42,(LINES-27)/2,(COLS-42)/2);
        box(disp_window,0,0);
        nodelay(disp_window,true);

        stat_window = newwin(5, 82, (LINES - 7),(COLS-82)/2);
        box(stat_window,0,0);


        cursesmode = true;
    }
    clear();
    wrefresh(disp_window);
        
    for(int y=0;y<25;y++) {
        for(int x=0;x<40;x++) {
            WORD address = (y*40)+x;
            mvwaddch(disp_window,y+1,x+1,displaymem[address]);
        }
    }

    wrefresh(disp_window);

    return true;
}

bool backspace_pressed() {
    if (!cursesmode) return false;

    int ch = wgetch(disp_window);

    if (ch == KEY_BACKSPACE) return true;

    return false;
}

bool init_io() {
    bool ok = true;
    if (displaymem == NULL) {
        ok = ok && (memory != NULL);
        if (ok) {
           displaymem = memory + 0xC000;
        }
    }
    ok = ok && clear_display();
    ok = ok && write_string(0,0,"Ready");
    ok = ok && display();

    return ok;
}

bool close_io() {
    if (cursesmode) {
        delwin(disp_window);
        endwin();
        cursesmode = false;
    }

    return !cursesmode;
}

bool display_test() {
    bool ok = true;

    ok = ok && init_io();

    int i = 0x00;
    for(int loop=0; ok && loop<100; loop++) {
        for( int x=0; ok && x<40; x++ ) {
            for( int y=0; ok && y<25; y++ ) {
                char ch = 0x41 + (i++%26);
                ok = ok && write_char(x,y,ch);
            }
        }
        ok = ok && display();
    }

    if (!ok) {
        if (cursesmode) {
            clear_display();
            write_string(0,0,"Display test failed");
            display();
        } else {
            fprintf(stderr,"Display test failed");
        }
    } else {
        clear_display();
        write_string(0,23,"Ready");
        write_string(0,24,"Display tests succeeded");
        display();
    }

    for(BYTE l=0; l<23;l++) {
        scroll_display(1);
        display();
    }
}
