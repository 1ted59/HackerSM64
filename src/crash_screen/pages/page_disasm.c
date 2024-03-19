#include <ultra64.h>

#include <string.h>

#include "types.h"
#include "sm64.h"

#include "crash_screen/util/insn_disasm.h"
#include "crash_screen/util/map_parser.h"
#include "crash_screen/util/memory_read.h"
#include "crash_screen/crash_controls.h"
#include "crash_screen/crash_draw.h"
#include "crash_screen/crash_main.h"
#include "crash_screen/crash_settings.h"
#include "crash_screen/crash_pages.h"
#include "crash_screen/crash_print.h"

#include "crash_screen/popups/popup_address_select.h"

#include "page_disasm.h"

#ifdef UNF
#include "usb/usb.h"
#include "usb/debug.h"
#endif // UNF


const char* sValNames_branch_arrow[] = {
    [DISASM_ARROW_MODE_OFF      ] = "OFF",
    [DISASM_ARROW_MODE_SELECTION] = "SELECTION",
#ifdef INCLUDE_DEBUG_MAP
    [DISASM_ARROW_MODE_FUNCTION ] = "FUNCTION",
#endif // INCLUDE_DEBUG_MAP
    [DISASM_ARROW_MODE_OVERSCAN ] = "OVERSCAN", //! TODO: Implement this mode.
};

#ifdef INCLUDE_DEBUG_MAP
    #define DISASM_ARROW_MODE_DEFAULT   DISASM_ARROW_MODE_FUNCTION
#else // !INCLUDE_DEBUG_MAP
    #define DISASM_ARROW_MODE_DEFAULT   DISASM_ARROW_MODE_SELECTION
#endif // !INCLUDE_DEBUG_MAP

struct CSSetting cs_settings_group_page_disasm[] = {
    [CS_OPT_HEADER_PAGE_DISASM      ] = { .type = CS_OPT_TYPE_HEADER,  .name = "DISASM",                         .valNames = &gValNames_bool,          .val = SECTION_EXPANDED_DEFAULT,  .defaultVal = SECTION_EXPANDED_DEFAULT,  .lowerBound = FALSE,                 .upperBound = TRUE,                       },
    [CS_OPT_DISASM_SHOW_RANGE       ] = { .type = CS_OPT_TYPE_SETTING, .name = "Show current address range",     .valNames = &gValNames_bool,          .val = TRUE,                      .defaultVal = TRUE,                      .lowerBound = FALSE,                 .upperBound = TRUE,                       },
    [CS_OPT_DISASM_SHOW_SYMBOL      ] = { .type = CS_OPT_TYPE_SETTING, .name = "Show current symbol name",       .valNames = &gValNames_bool,          .val = TRUE,                      .defaultVal = TRUE,                      .lowerBound = FALSE,                 .upperBound = TRUE,                       },
    [CS_OPT_DISASM_BINARY           ] = { .type = CS_OPT_TYPE_SETTING, .name = "Unknown as binary",              .valNames = &gValNames_bool,          .val = FALSE,                     .defaultVal = FALSE,                     .lowerBound = FALSE,                 .upperBound = TRUE,                       },
    [CS_OPT_DISASM_PSEUDOINSNS      ] = { .type = CS_OPT_TYPE_SETTING, .name = "Pseudo-instructions",            .valNames = &gValNames_bool,          .val = TRUE,                      .defaultVal = TRUE,                      .lowerBound = FALSE,                 .upperBound = TRUE,                       },
    [CS_OPT_DISASM_IMM_FMT          ] = { .type = CS_OPT_TYPE_SETTING, .name = "Immediates format",              .valNames = &gValNames_print_num_fmt, .val = PRINT_NUM_FMT_HEX,         .defaultVal = PRINT_NUM_FMT_HEX,         .lowerBound = PRINT_NUM_FMT_HEX,     .upperBound = PRINT_NUM_FMT_DEC,          },
    [CS_OPT_DISASM_OFFSET_ADDR      ] = { .type = CS_OPT_TYPE_SETTING, .name = "Offsets as addresses",           .valNames = &gValNames_bool,          .val = FALSE,                     .defaultVal = FALSE,                     .lowerBound = FALSE,                 .upperBound = TRUE,                       },
    [CS_OPT_DISASM_ARROW_MODE       ] = { .type = CS_OPT_TYPE_SETTING, .name = "Branch arrow mode",              .valNames = &sValNames_branch_arrow,  .val = DISASM_ARROW_MODE_DEFAULT, .defaultVal = DISASM_ARROW_MODE_DEFAULT, .lowerBound = DISASM_ARROW_MODE_OFF, .upperBound = DISASM_ARROW_MODE_OVERSCAN, },
    [CS_OPT_END_DISASM              ] = { .type = CS_OPT_TYPE_END, },
};


const enum ControlTypes cs_cont_list_disasm[] = {
    CONT_DESC_SWITCH_PAGE,
    CONT_DESC_PAGE_SELECT,
    CONT_DESC_SHOW_CONTROLS,
    CONT_DESC_HIDE_CRASH_SCREEN,
#ifdef UNF
    CONT_DESC_OS_PRINT,
#endif // UNF
    CONT_DESC_CURSOR_VERTICAL,
    CONT_DESC_JUMP_TO_ADDRESS,
#ifdef INCLUDE_DEBUG_MAP
    CONT_DESC_TOGGLE_FUNCTIONS,
#endif // INCLUDE_DEBUG_MAP
    CONT_DESC_LIST_END,
};


#define DISASM_NUM_SHOWN_ROWS 21

static u32 sDisasmViewportIndex = 0x00000000;
static u32 sDisasmBranchStartX = 0; // The X position where branch arrows start.
static u32 sDisasmNumShownRows = DISASM_NUM_SHOWN_ROWS;

#ifdef INCLUDE_DEBUG_MAP
static const RGBA32 sBranchColors[] = {
    COLOR_RGBA32_ORANGE,
    COLOR_RGBA32_LIME,
    COLOR_RGBA32_CYAN,
    COLOR_RGBA32_MAGENTA,
    COLOR_RGBA32_YELLOW,
    COLOR_RGBA32_PINK,
    COLOR_RGBA32_LIGHT_GRAY,
    COLOR_RGBA32_LIGHT_BLUE,
};

_Bool gFillBranchBuffer = FALSE;
static _Bool sContinueFillBranchBuffer = FALSE;

ALIGNED16 static BranchArrow sBranchArrows[DISASM_BRANCH_BUFFER_SIZE];
static u32 sNumBranchArrows = 0;

static Address sBranchBufferCurrAddr = 0x00000000;


void reset_branch_buffer(Address funcAddr) {
    bzero(sBranchArrows, sizeof(sBranchArrows));
    sNumBranchArrows = 0;

    sBranchBufferCurrAddr = funcAddr;
}
#endif // INCLUDE_DEBUG_MAP

void page_disasm_init(void) {
    sDisasmViewportIndex = gSelectedAddress;

#ifdef INCLUDE_DEBUG_MAP
    gFillBranchBuffer         = FALSE;
    sContinueFillBranchBuffer = FALSE;
    reset_branch_buffer((Address)NULL);
#endif // INCLUDE_DEBUG_MAP
}

#ifdef INCLUDE_DEBUG_MAP
//! TODO: Optimize this as much as possible.
//! TODO: Version that checks for branches relative to viewport (overscan).
// @returns whether to continue next frame.
_Bool disasm_fill_branch_buffer(const char* fname, Address funcAddr) {
    if (fname == NULL) {
        return FALSE;
    }

    s16 curBranchColorIndex;
    s32 curBranchX;

    if (sNumBranchArrows == 0) {
        // Start:
        curBranchColorIndex = 0;
        curBranchX = sDisasmBranchStartX;
    } else { //! TODO: Verify that this ordering is correct:
        // Continue:
        curBranchColorIndex = sBranchArrows[sNumBranchArrows - 1].colorIndex;
        curBranchX          = sBranchArrows[sNumBranchArrows - 1].xPos;
    }

    // Pick up where we left off.
    BranchArrow* currArrow = &sBranchArrows[sNumBranchArrows];

    OSTime startTime = osGetTime();
    while (TRUE) {
        // Too many entries searched.
        if (sBranchBufferCurrAddr > (funcAddr + DISASM_FUNCTION_SEARCH_MAX_OFFSET)) {
            return FALSE;
        }

        // Too many arrows for buffer.
        if (sNumBranchArrows >= DISASM_BRANCH_BUFFER_SIZE) {
            return FALSE;
        }

        // Check if we have left the function.
        const MapSymbol* symbol = get_map_symbol(sBranchBufferCurrAddr, SYMBOL_SEARCH_FORWARD);
        if (symbol != NULL) {
            if (!is_in_code_segment(symbol->addr)) {
                return FALSE;
            }
            if (funcAddr != symbol->addr) {
                return FALSE;
            }
        }

        // Get the offset for the current function.
        InsnData insn = {
            .raw = *(Word*)sBranchBufferCurrAddr, //! TODO: Is this an unsafe read?
        };
        s16 branchOffset = insn_check_for_branch_offset(insn);

        if (branchOffset != 0x0000) { //! TODO: Verify ordering:
            curBranchX += (DISASM_BRANCH_ARROW_SPACING + 1);
            curBranchColorIndex = ((curBranchColorIndex + 1) % ARRAY_COUNT(sBranchColors));

            // Wrap around if extended past end of screen.
            if ((sDisasmBranchStartX + curBranchX) > CRASH_SCREEN_TEXT_X2) {
                curBranchX = (DISASM_BRANCH_ARROW_HEAD_SIZE + DISASM_BRANCH_ARROW_HEAD_OFFSET);
            }

            currArrow->startAddr    = sBranchBufferCurrAddr;
            currArrow->branchOffset = branchOffset;
            currArrow->colorIndex   = curBranchColorIndex;
            currArrow->xPos         = curBranchX;

            currArrow++;
            sNumBranchArrows++;
        }

        sBranchBufferCurrAddr += PAGE_DISASM_STEP;

        // If branch mapping takes longer than a frame, so continue from the same place on the next frame.
        if ((osGetTime() - startTime) > FRAMES_TO_CYCLES(1)) { // Is there a better way to do this?
            return TRUE;
        }
    }

    return FALSE;
}
#endif // INCLUDE_DEBUG_MAP

void draw_branch_arrow(s32 startLine, s32 endLine, s32 dist, RGBA32 color, u32 printLine) {
    s32 numShownRows = sDisasmNumShownRows;

    // Check to see if arrow is fully away from the screen.
    if (
        ((startLine >= 0           ) || (endLine >= 0           )) &&
        ((startLine <  numShownRows) || (endLine <  numShownRows))
    ) {
        s32 arrowStartHeight = (TEXT_Y(printLine + startLine) + 3);
        s32 arrowEndHeight   = (TEXT_Y(printLine +   endLine) + 3);

        if (startLine < 0) {
            arrowStartHeight = (TEXT_Y(printLine) - 1);
        } else if (startLine >= numShownRows) {
            arrowStartHeight = (TEXT_Y(printLine + numShownRows) - 2);
        } else {
            cs_draw_rect((sDisasmBranchStartX + 1), arrowStartHeight, dist, 1, color);
        }

        if (endLine < 0) {
            arrowEndHeight = (TEXT_Y(printLine) - 1);
        } else if (endLine >= numShownRows) {
            arrowEndHeight = (TEXT_Y(printLine + numShownRows) - 2);
        } else {
            const u32 startX = ((sDisasmBranchStartX + dist) - DISASM_BRANCH_ARROW_HEAD_OFFSET);

            cs_draw_triangle(
                (startX - DISASM_BRANCH_ARROW_HEAD_SIZE), (arrowEndHeight - DISASM_BRANCH_ARROW_HEAD_SIZE),
                DISASM_BRANCH_ARROW_HEAD_SIZE, (DISASM_BRANCH_ARROW_HEAD_SIZE * 2),
                color, CS_TRI_LEFT
            );
            cs_draw_rect(
                startX, arrowEndHeight,
                (DISASM_BRANCH_ARROW_HEAD_OFFSET + 1), 1,
                color
            );
        }

        s32 height = absi(arrowEndHeight - arrowStartHeight);

        // Middle of arrow.
        cs_draw_rect((sDisasmBranchStartX + dist), MIN(arrowStartHeight, arrowEndHeight), 1, height, color);
    }
}

#ifdef INCLUDE_DEBUG_MAP
void disasm_draw_branch_arrows(u32 printLine) {
    // Draw branch arrows from the buffer.
    BranchArrow* currArrow = &sBranchArrows[0];

    for (u32 i = 0; i < sNumBranchArrows; i++) {
        s32 startLine = (((s32)currArrow->startAddr - (s32)sDisasmViewportIndex) / PAGE_DISASM_STEP);
        s32 endLine = (startLine + currArrow->branchOffset + 1);

        draw_branch_arrow(startLine, endLine, currArrow->xPos, sBranchColors[currArrow->colorIndex], printLine);

        currArrow++;
    }

    osWritebackDCacheAll();
}
#endif // INCLUDE_DEBUG_MAP

void print_as_insn(const u32 charX, const u32 charY, const Address addr, const Word data) {
    const char* destFname = NULL;
    const char* insnAsStr = cs_insn_to_string(addr, (InsnData)data, &destFname, TRUE);

    // "[instruction name] [params]"
    cs_print(charX, charY, "%s", insnAsStr);

#ifdef INCLUDE_DEBUG_MAP
    if (cs_get_setting_val(CS_OPT_GROUP_GLOBAL, CS_OPT_GLOBAL_SYMBOL_NAMES) && (destFname != NULL)) {
        // "[function name]"
        cs_print_scroll((charX + TEXT_WIDTH(INSN_NAME_DISPLAY_WIDTH)), charY,
            (CRASH_SCREEN_NUM_CHARS_X - (INSN_NAME_DISPLAY_WIDTH)),
            STR_COLOR_PREFIX"%s",
            COLOR_RGBA32_CRASH_FUNCTION_NAME, destFname
        );
    }
#endif // INCLUDE_DEBUG_MAP
}

static void print_as_binary(const u32 charX, const u32 charY, const Word data, RGBA32 color) { //! TODO: make this a custom formatting specifier?, maybe \%b?
    u32 bitCharX = charX;

    for (u32 c = 0; c < SIZEOF_BITS(Word); c++) {
        if ((c % SIZEOF_BITS(Byte)) == 0) { // Space between each byte.
            bitCharX += TEXT_WIDTH(1);
        }

        cs_draw_glyph(bitCharX, charY, (((data >> (SIZEOF_BITS(Word) - c)) & 0b1) ? '1' : '0'), color);

        bitCharX += TEXT_WIDTH(1);
    }
}

static void disasm_draw_asm_entries(u32 line, u32 numLines, Address selectedAddr, Address pc) {
    const enum CSDisasmBranchArrowModes branchArrowMode = cs_get_setting_val(CS_OPT_GROUP_PAGE_DISASM, CS_OPT_DISASM_ARROW_MODE);
    const _Bool unkAsBinary = cs_get_setting_val(CS_OPT_GROUP_PAGE_DISASM, CS_OPT_DISASM_BINARY);

    u32 charX = TEXT_X(0);
    u32 charY = TEXT_Y(line);

    for (u32 y = 0; y < numLines; y++) {
        Address addr = (sDisasmViewportIndex + (y * PAGE_DISASM_STEP));
        charY = TEXT_Y(line + y);

        // Draw crash and selection rectangles:
        if (addr == pc) {
            // Draw a red selection rectangle.
            cs_draw_row_crash_box(charY);
            // "<-- CRASH"
            cs_print((CRASH_SCREEN_TEXT_X2 - TEXT_WIDTH(STRLEN("<-- CRASH"))), charY, STR_COLOR_PREFIX"<-- CRASH", COLOR_RGBA32_CRASH_AT);
        }
        if (addr == selectedAddr) {
            // Draw a gray selection rectangle.
            cs_draw_row_selection_box(charY);
        }

        Word data = 0x00000000;
        if (!try_read_word_aligned(&data, addr)) {
            cs_print(charX, charY, (STR_COLOR_PREFIX"*"), COLOR_RGBA32_CRASH_OUT_OF_BOUNDS);
        } else if (is_in_code_segment(addr)) {
            print_as_insn(charX, charY, addr, data);

            if ((addr == selectedAddr) && (branchArrowMode == DISASM_ARROW_MODE_SELECTION)) {
                InsnData insn = {
                    .raw = data,
                };
                s16 branchOffset = insn_check_for_branch_offset(insn);

                if (branchOffset != 0x0000) {
                    draw_branch_arrow(y, (y + branchOffset + 1), (DISASM_BRANCH_ARROW_HEAD_SIZE + DISASM_BRANCH_ARROW_HEAD_OFFSET), COLOR_RGBA32_ORANGE, line);
                }
            }
        } else { // Outside of code segments:
            if (unkAsBinary) {
                // "bbbbbbbb bbbbbbbb bbbbbbbb bbbbbbbb"
                print_as_binary(charX, charY, data, COLOR_RGBA32_WHITE);
                // cs_print(charX, charY, "%032b", data);
            } else {
                // "[XXXXXXXX]"
                cs_print(charX, charY, STR_HEX_WORD, data);
            }
        }
    }

    osWritebackDCacheAll();
}

//! TODO: automatically check page/address change:
// Address sCurrFuncAddr = 0x00000000;
// const char* sCurrFuncName = NULL;

void page_disasm_draw(void) {
    __OSThreadContext* tc = &gInspectThread->context;
    Address alignedSelectedAddr = ALIGNFLOOR(gSelectedAddress, PAGE_DISASM_STEP);
    Address epc = GET_EPC(tc);

    sDisasmNumShownRows = DISASM_NUM_SHOWN_ROWS;
    const _Bool showCurrentRange  = cs_get_setting_val(CS_OPT_GROUP_PAGE_DISASM, CS_OPT_DISASM_SHOW_RANGE);
    sDisasmNumShownRows -= showCurrentRange;
    const _Bool showCurrentSymbol = cs_get_setting_val(CS_OPT_GROUP_PAGE_DISASM, CS_OPT_DISASM_SHOW_SYMBOL);
    sDisasmNumShownRows -= showCurrentSymbol;

    sDisasmBranchStartX = (DISASM_BRANCH_ARROW_HEAD_SIZE + DISASM_BRANCH_ARROW_HEAD_OFFSET) +
                        cs_get_setting_val(CS_OPT_GROUP_PAGE_DISASM, CS_OPT_DISASM_OFFSET_ADDR)
                        ? TEXT_X(INSN_NAME_DISPLAY_WIDTH + STRLEN("R0, R0, 0x80XXXXXX"))
                        : TEXT_X(INSN_NAME_DISPLAY_WIDTH + STRLEN("R0, R0, +0x0000"));

    u32 line = 1;

    Address startAddr = sDisasmViewportIndex;
    Address endAddr   = (startAddr + ((sDisasmNumShownRows - 1) * PAGE_DISASM_STEP));

    if (showCurrentRange) {
        // "[XXXXXXXX] in [XXXXXXXX]-[XXXXXXXX]"
        cs_print(TEXT_X(0), TEXT_Y(line),
            (STR_COLOR_PREFIX STR_HEX_WORD" in "STR_HEX_WORD"-"STR_HEX_WORD),
            COLOR_RGBA32_WHITE, alignedSelectedAddr, startAddr, endAddr
        );
        line++;
    }

    if (showCurrentSymbol) {
        cs_print_addr_location_info(TEXT_X(0), TEXT_Y(line), CRASH_SCREEN_NUM_CHARS_X, alignedSelectedAddr, TRUE);
        line++;
    }

    if (cs_get_setting_val(CS_OPT_GROUP_PAGE_DISASM, CS_OPT_DISASM_ARROW_MODE) == DISASM_ARROW_MODE_FUNCTION) {
        disasm_draw_branch_arrows(line);
    }

    disasm_draw_asm_entries(line, sDisasmNumShownRows, alignedSelectedAddr, epc);

    if (showCurrentRange || showCurrentSymbol) {
        cs_draw_divider(DIVIDER_Y(line));
    }

    u32 line2 = (line + sDisasmNumShownRows);

    cs_draw_divider(DIVIDER_Y(line2));

    u32 scrollTop = (DIVIDER_Y(line) + 1);
    u32 scrollBottom = DIVIDER_Y(line2);

    const size_t shownSection = ((sDisasmNumShownRows - 1) * PAGE_DISASM_STEP);

    // Scroll bar:
    cs_draw_scroll_bar(
        scrollTop, scrollBottom,
        shownSection, VIRTUAL_RAM_SIZE,
        (sDisasmViewportIndex - VIRTUAL_RAM_START),
        COLOR_RGBA32_CRASH_SCROLL_BAR, TRUE
    );

    // Scroll bar crash position marker:
    cs_draw_scroll_bar(
        scrollTop, scrollBottom,
        shownSection, VIRTUAL_RAM_SIZE,
        (epc - VIRTUAL_RAM_START),
        COLOR_RGBA32_CRASH_AT, FALSE
    );

    osWritebackDCacheAll();
}

static void disasm_move_up(void) {
    gSelectedAddress = ALIGNFLOOR(gSelectedAddress, PAGE_DISASM_STEP);
    // Scroll up.
    if (gSelectedAddress >= (VIRTUAL_RAM_START + PAGE_DISASM_STEP))  {
        gSelectedAddress -= PAGE_DISASM_STEP;
    }
}

static void disasm_move_down(void) {
    gSelectedAddress = ALIGNFLOOR(gSelectedAddress, PAGE_DISASM_STEP);
    // Scroll down.
    if (gSelectedAddress <= (VIRTUAL_RAM_END - PAGE_DISASM_STEP)) {
        gSelectedAddress += PAGE_DISASM_STEP;
    }
}

void page_disasm_input(void) {
#ifdef INCLUDE_DEBUG_MAP
    Address oldPos = gSelectedAddress;
#endif // INCLUDE_DEBUG_MAP

    if (gCSDirectionFlags.pressed.up) {
        disasm_move_up();
    }

    if (gCSDirectionFlags.pressed.down) {
        disasm_move_down();
    }

    u16 buttonPressed = gCSCompositeController->buttonPressed;

    if (buttonPressed & A_BUTTON) {
        open_address_select(get_insn_branch_target_from_addr(gSelectedAddress));
    }

    sDisasmViewportIndex = cs_clamp_view_to_selection(sDisasmViewportIndex, gSelectedAddress, sDisasmNumShownRows, PAGE_DISASM_STEP);

#ifdef INCLUDE_DEBUG_MAP
    if (buttonPressed & B_BUTTON) {
        cs_inc_setting(CS_OPT_GROUP_GLOBAL, CS_OPT_GLOBAL_SYMBOL_NAMES, TRUE);
    }

    if (cs_get_setting_val(CS_OPT_GROUP_PAGE_DISASM, CS_OPT_DISASM_ARROW_MODE) == DISASM_ARROW_MODE_FUNCTION) {
        //! TODO: don't reset branch buffer if switched page back into the same function.
        if (gCSSwitchedPage || (get_symbol_index_from_addr_forward(oldPos) != get_symbol_index_from_addr_forward(gSelectedAddress))) {
            gFillBranchBuffer = TRUE;
        }

        Address alignedSelectedAddress = ALIGNFLOOR(gSelectedAddress, PAGE_DISASM_STEP);

        const MapSymbol* symbol = get_map_symbol(alignedSelectedAddress, SYMBOL_SEARCH_FORWARD);
        if (symbol != NULL) {
            const char* fname = get_map_symbol_name(symbol);

            //! TODO: fname null check.

            if (gFillBranchBuffer) {
                gFillBranchBuffer = FALSE;
                reset_branch_buffer(symbol->addr);
                sContinueFillBranchBuffer = TRUE;
            }

            if (sContinueFillBranchBuffer) {
                sContinueFillBranchBuffer = disasm_fill_branch_buffer(fname, symbol->addr);
            }
        } else {
            gFillBranchBuffer = FALSE;
            reset_branch_buffer(alignedSelectedAddress);
            sContinueFillBranchBuffer = FALSE;
        }
    }
#endif // INCLUDE_DEBUG_MAP
}

void page_disasm_print(void) {
#ifdef UNF
    osSyncPrintf("\n");

    Address startAddr = sDisasmViewportIndex;
    Address endAddr   = (startAddr + ((sDisasmNumShownRows - 1) * PAGE_DISASM_STEP));

    osSyncPrintf("SECTION: ["STR_HEX_WORD"-"STR_HEX_WORD"]\n", startAddr, endAddr);

    for (u32 y = 0; y < sDisasmNumShownRows; y++) {
        Address addr = (startAddr + (y * PAGE_DISASM_STEP));
        osSyncPrintf("- ["STR_HEX_WORD"]: ", addr);
        Word data = 0x00000000;
        if (!try_read_word_aligned(&data, addr)) {
            osSyncPrintf("*");
        } else if (is_in_code_segment(addr)) {
            const char* destFname = NULL;
            const char* insnAsStr = cs_insn_to_string(addr, (InsnData)data, &destFname, FALSE);

            osSyncPrintf("%s", insnAsStr);
 #ifdef INCLUDE_DEBUG_MAP
            if (cs_get_setting_val(CS_OPT_GROUP_GLOBAL, CS_OPT_GLOBAL_SYMBOL_NAMES) && (destFname != NULL)) {
                osSyncPrintf("%s", destFname);
            }
 #endif // INCLUDE_DEBUG_MAP
            __OSThreadContext* tc = &gInspectThread->context;
            if (addr == GET_EPC(tc)) {
                osSyncPrintf("<-- CRASH");
            }
        } else { // Outside of code segments:
            osSyncPrintf(STR_HEX_WORD, data);
        }

        osSyncPrintf("\n");
    }
#endif // UNF
}


struct CSPage gCSPage_disasm = {
    .name         = "DISASM",
    .initFunc     = page_disasm_init,
    .drawFunc     = page_disasm_draw,
    .inputFunc    = page_disasm_input,
    .printFunc    = page_disasm_print,
    .contList     = cs_cont_list_disasm,
    .settingsList = cs_settings_group_page_disasm,
    .flags = {
        .initialized = FALSE,
        .crashed     = FALSE,
    },
};
