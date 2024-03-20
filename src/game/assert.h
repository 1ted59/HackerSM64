#pragma once

#include <ultra64.h>

#include "types.h"

#include "asm.h"


#define ASSERTF_BUFFER_SIZE 255


extern const char* __n64Assert_Condition;
extern const char* __n64Assert_Filename;
extern int         __n64Assert_LineNum;
extern const char* __n64Assert_Message;


extern int __assert_address;


extern void __n64Assert(char* condition, char* fileName, u32 lineNum, char* message);
extern void __n64Assertf(char* condition, char* fileName, u32 lineNum, char* message, ...) __attribute__((format(printf, 4, 5)));


/**
 * Set the assert address to the current location.
 */
#define SET_ASSERT_ADDRESS() __assert_address = _asm_getaddr()


/**
 * Will always cause a crash with your message of choice.
 */
#define ERROR(message) do {                                                     \
    SET_ASSERT_ADDRESS();                                                       \
    __n64Assert(NULL, __FILE__, __LINE__, (message));                           \
} while (0)

#define ERRORF(message, ...) do {                                               \
    SET_ASSERT_ADDRESS();                                                       \
    __n64Assertf(NULL, __FILE__, __LINE__, (message), ##__VA_ARGS__);           \
} while (0)


/**
 * Will always cause a crash if cond is not true (handle with care).
 */
#define ASSERT(condition, message) do {                                         \
    if (!(condition)) {                                                         \
        SET_ASSERT_ADDRESS();                                                   \
        __n64Assert(#condition, __FILE__, __LINE__, (message));                 \
    }                                                                           \
} while (0)
#define ASSERTF(condition, message, ...) do {                                   \
    if (!(condition)) {                                                         \
        SET_ASSERT_ADDRESS();                                                   \
        __n64Assertf(#condition, __FILE__, __LINE__, (message), ##__VA_ARGS__); \
    }                                                                           \
} while (0)

/**
 * Will cause a crash if condition is not true, and DEBUG_ASSERTIONS is defined (allows for quick removal of littered asserts).
 */
#ifdef DEBUG_ASSERTIONS
    #define DEBUG_ERROR(message)                    ERROR(message)
    #define DEBUG_ERRORF(message, ...)              ERRORF((message), ##__VA_ARGS__)
    #define DEBUG_ASSERT(condition, message)        ASSERT(condition, (message))
    #define DEBUG_ASSERTF(condition, message, ...)  ASSERTF(condition, (message), ##__VA_ARGS__)
#else
    #define DEBUG_ERROR(message)
    #define DEBUG_ERRORF(message, ...)
    #define DEBUG_ASSERT(condition, message)
    #define DEBUG_ASSERTF(condition, message, ...)
#endif

// Case sensitivity:
#define error           ERROR
#define errorf          ERRORF
#define assert          ASSERT
#define assertf         ASSERTF
#define debug_error     DEBUG_ERROR
#define debug_errorf    DEBUG_ERRORF
#undef debug_assert // Overwrite the UNF version.
#define debug_assert    DEBUG_ASSERT
#define debug_assertf   DEBUG_ASSERTF

// Backwards compatibility:
#define aggress         assert
#define aggressf        assertf
