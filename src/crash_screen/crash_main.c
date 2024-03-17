#include <ultra64.h>

#include <PR/os_internal_error.h>

#include <stdarg.h>
#include <string.h>

#include "types.h"
#include "sm64.h"

#include "util/map_parser.h"
#include "crash_controls.h"
#include "crash_draw.h"
#include "crash_pages.h"
#include "crash_settings.h"

#include "crash_main.h"

#include "audio/external.h"
#include "buffers/framebuffers.h"
#include "buffers/zbuffer.h"
#include "game/main.h"
#ifdef UNF
#include "usb/usb.h"
#include "usb/debug.h"
#endif // UNF


ALIGNED16 static struct CSThreadInfo sCSThreadInfos[NUM_CRASH_SCREEN_BUFFERS]; // Crash screen threads.
static s32   sCSThreadIndex = 0;    // Crash screen thread index.
static _Bool sFirstCrash    = TRUE; // Used to make certain things only happen on the first crash.

CSThreadInfo* gActiveCSThreadInfo = NULL; // Pointer to the current crash screen thread info.
OSThread*     gCrashedThread      = NULL; // Pointer to the most recently crashed thread.
OSThread*     gInspectThread      = NULL; // Pointer to the thread the crash screen will be inspecting. //! TODO: Allow changing inspected thread.

Address gSetCrashAddress = 0x00000000; // Used by SET_CRASH_PTR to set the crashed thread PC.
Address gSelectedAddress = 0x00000000; // Selected address for ram viewer and disasm pages.


/**
 * @brief Reinitialize all of the crash screen's pages.
 */
void cs_reinitialize_pages(void) {
    for (int pageID = 0; pageID < ARRAY_COUNT(gCSPages); pageID++) {
        gCSPages[pageID]->flags.initialized = FALSE;
    }
}

/**
 * @brief Reinitialize the crash screen's global variables, settings, buffers, etc.
 */
static void cs_reinitialize(void) {
    // If the crash screen has crashed, disable the page that crashed, unless it was an assert.
    if (!sFirstCrash && (gCrashedThread->context.cause != EXC_SYSCALL)) {
        cs_get_current_page()->flags.crashed = TRUE;
    }

    gCSPageID        = CRASH_SCREEN_START_PAGE;
    gCSSwitchedPage  = FALSE;
    gCSPopupID       = CS_POPUP_NONE;
    gCSSwitchedPopup = FALSE;

    if (sFirstCrash) {
        cs_settings_apply_func_to_all(cs_setting_func_reset);
    }
    cs_settings_set_all_headers(FALSE);

    gSelectedAddress = 0x00000000;

    gCSDirectionFlags.raw = 0b00000000;

    cs_reinitialize_pages();
}

/**
 * @brief Iterates through the active thread queue for a user thread with either
 *        the CPU break or Fault flag set.
 *
 * @return OSThread* The crashed thread.
 */
static OSThread* get_crashed_thread(void) {
    OSThread* thread = __osGetCurrFaultedThread();

    while (
        (thread != NULL) &&
        (thread->priority != OS_PRIORITY_THREADTAIL) // OS_PRIORITY_THREADTAIL indicates the end of the thread queue.
    ) {
        if (
            (thread->priority > OS_PRIORITY_IDLE  ) &&
            (thread->priority < OS_PRIORITY_APPMAX) && //! TODO: Should this include OS_PRIORITY_APPMAX threads? Official N64 games don't.
            (thread->flags & (OS_FLAG_CPU_BREAK | OS_FLAG_FAULT)) &&
            (thread != gCrashedThread)
        ) {
            return thread;
        }

        thread = thread->tlnext;
    }

    return NULL;
}

#ifdef FUNNY_CRASH_SOUND
/**
 * @brief Pause the current thread for a specific amount of time.
 *
 * @param[in] ms Number of milliseconds to wait.
 */
void cs_sleep(u32 ms) {
    OSTime cycles = (((ms * 1000LL) * osClockRate) / 1000000ULL);
    osSetTime(0);
    while (osGetTime() < cycles) {}
}

extern struct SequenceQueueItem sBackgroundMusicQueue[6];
extern void audio_signal_game_loop_tick(void);
extern void stop_sounds_in_continuous_banks(void);

/**
 * @brief Play a sound.
 *
 * @param[out] threadInfo Pointer to the thread info.
 * @param[in ] sound      The sound ID to play.
 */
void cs_play_sound(struct CSThreadInfo* threadInfo, s32 sound) {
    threadInfo->thread.priority = 15;
    stop_sounds_in_continuous_banks();
    stop_background_music(sBackgroundMusicQueue[0].seqId);
    audio_signal_game_loop_tick();
    cs_sleep(200);
    play_sound(sound, gGlobalSoundSource);
    audio_signal_game_loop_tick();
    cs_sleep(200);
}
#endif // FUNNY_CRASH_SOUND

/**
 * @brief Runs once on every crash.
 *
 * @param[in,out] threadInfo Pointer to the thread info.
 */
static void on_crash(struct CSThreadInfo* threadInfo) {
    // Create another crash screen thread in case the current one crashes.
    create_crash_screen_thread();

    // Set the active thread info pointer.
    gActiveCSThreadInfo = threadInfo;

    // Reinitialize global variables, settings, buffers, etc.
    cs_reinitialize();

    osViSetEvent(&threadInfo->mesgQueue, (OSMesg)CRASH_SCREEN_MSG_VI_VBLANK, 1);

#ifdef FUNNY_CRASH_SOUND
    cs_play_sound(threadInfo, SOUND_MARIO_WAAAOOOW);
#endif // FUNNY_CRASH_SOUND

    __OSThreadContext* tc = &gInspectThread->context;

    // Default to disasm page if the crash was caused by an Illegal Instruction.
    if (tc->cause == EXC_II) {
        cs_set_page(PAGE_DISASM);
    }

    // Only on the first crash:
    if (sFirstCrash) {
        sFirstCrash = FALSE;

        // If a position was specified, use that.
        if (gSetCrashAddress != 0x0) {
            tc->pc = gSetCrashAddress;
            gSetCrashAddress = 0x00000000;
            cs_set_page(PAGE_RAM_VIEWER);
        }

        // Use the Z buffer's memory space to save a screenshot of the game.
        cs_take_screenshot_of_game(gZBuffer, sizeof(gZBuffer));

#ifdef INCLUDE_DEBUG_MAP
        map_data_init();
#endif // INCLUDE_DEBUG_MAP

#ifdef UNF
        cs_os_print_page(cs_get_current_page());
#endif // UNF
    }

    gSelectedAddress = tc->pc;
}

/**
 * @brief Crash screen tread function. Waits for a crash then loops the crash screen.
 *
 * @param[in] arg Unused arg.
 */
void crash_screen_thread_entry(UNUSED void* arg) {
    struct CSThreadInfo* threadInfo = &sCSThreadInfos[sCSThreadIndex];
    OSThread* crashedThread = NULL;

    // Increment the current thread index.
    sCSThreadIndex = ((sCSThreadIndex + 1) % ARRAY_COUNT(sCSThreadInfos));

    // Check for CPU, SP, and MSG crashes.
    osSetEventMesg(OS_EVENT_CPU_BREAK, &threadInfo->mesgQueue, (OSMesg)CRASH_SCREEN_MSG_CPU_BREAK);
    osSetEventMesg(OS_EVENT_SP_BREAK,  &threadInfo->mesgQueue, (OSMesg)CRASH_SCREEN_MSG_SP_BREAK );
    osSetEventMesg(OS_EVENT_FAULT,     &threadInfo->mesgQueue, (OSMesg)CRASH_SCREEN_MSG_FAULT    );

    // Wait for one of the above types of break or fault to occur.
    while (TRUE) {
        osRecvMesg(&threadInfo->mesgQueue, &threadInfo->mesg, OS_MESG_BLOCK);
        crashedThread = get_crashed_thread();
        if (crashedThread != NULL) {
            break;
        }
    }

    // -- A thread has crashed --

    gCrashedThread = crashedThread;
    gInspectThread = gCrashedThread;

    on_crash(threadInfo);

    // Crash screen open.
    while (TRUE) {
        cs_update_input();
        cs_draw_main();
    }
}

/**
 * @brief Create a crash screen thread.
 */
void create_crash_screen_thread(void) {
    struct CSThreadInfo* threadInfo = &sCSThreadInfos[sCSThreadIndex];
    bzero(threadInfo, sizeof(struct CSThreadInfo));

    osCreateMesgQueue(&threadInfo->mesgQueue, &threadInfo->mesg, 1);
    osCreateThread(
        &threadInfo->thread, (THREAD_1000_CRASH_SCREEN_0 + sCSThreadIndex),
        crash_screen_thread_entry, NULL,
        ((u8*)threadInfo->stack + sizeof(threadInfo->stack)), // Pointer to the end of the stack.
        (OS_PRIORITY_APPMAX - 1) //! TODO: Why shouldn't get_crashed_thread check for OS_PRIORITY_APPMAX threads?
    );
    osStartThread(&threadInfo->thread);
}
