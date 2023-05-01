#pragma once

#include <ultra64.h>

#include "types.h"
#include "game/main.h"


// The size of one row of the font image.
typedef u32 CSFontRow;


// Virtual RAM boundary defines.
#define VIRTUAL_RAM_START (Address)RAM_START
#define VIRTUAL_RAM_END   (Address)0xFFFFFFFF
#define VIRTUAL_RAM_SIZE  (size_t)(VIRTUAL_RAM_END - VIRTUAL_RAM_START)

// The number of crash screen threads that will be cycled through when the crash screen crashes. This should be at least 3.
#define NUM_CRASH_SCREEN_BUFFERS 3


struct CSThreadInfo {
    /*0x000*/ OSThread thread; /*0x1B0*/
    /*0x1B0*/ Register stack[THREAD2_STACK / sizeof(Register)]; /*0x400*/
    /*0x4B0*/ OSMesgQueue mesgQueue; /*0x18*/
    /*0x4C8*/ OSMesg mesg; /*0x04*/
}; /*0x4CC*/

struct CSPage {
    /*0x00*/ void (*initFunc)(void);
    /*0x04*/ void (*drawFunc)(void);
    /*0x08*/ void (*inputFunc)(void);
    /*0x0C*/ const enum ControlTypes* contList;
    /*0x10*/ const char* name;
    /*0x14*/ struct PACKED {
                /*0x00*/ u32             : 29;
                /*0x03*/ u32 printName   :  1;
                /*0x03*/ u32 crashed     :  1;
                /*0x03*/ u32 initialized :  1;
            } flags; /*0x04*/
}; /*0x18*/


// Time conversion macros
#define FPS_COUNT 30
#define FRAMES_TO_NESC(f)   (((OSTime)(f) * 1000000000LL) / FPS_COUNT)
#define FRAMES_TO_UESC(f)   (((OSTime)(f) * 1000000LL) / FPS_COUNT)
#define FRAMES_TO_CYCLES(f) (((OSTime)(f) * OS_CPU_COUNTER) / FPS_COUNT)
#define NSEC_TO_FRAMES(n)   (((OSTime)(n) * FPS_COUNT) / 1000000000LL)
#define USEC_TO_FRAMES(n)   (((OSTime)(n) * FPS_COUNT) / 1000000LL)
#define CYCLES_TO_FRAMES(c) (((OSTime)(c) * FPS_COUNT) / OS_CPU_COUNTER)
