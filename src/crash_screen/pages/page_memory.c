#include <ultra64.h>

#include "types.h"
#include "sm64.h"

#include "crash_screen/address_select.h"
#include "crash_screen/crash_controls.h"
#include "crash_screen/crash_draw.h"
#include "crash_screen/crash_main.h"
#include "crash_screen/crash_pages.h"
#include "crash_screen/crash_print.h"
#include "crash_screen/crash_settings.h"
#include "crash_screen/memory_read.h"

#include "page_memory.h"

#ifdef UNF
#include "usb/debug.h"
#endif // UNF


struct CSSetting cs_settings_group_page_memory[] = {
    [CS_OPT_HEADER_PAGE_MEMORY      ] = { .type = CS_OPT_TYPE_HEADER,  .name = "RAM VIEW",                       .valNames = &gValNames_bool,          .val = SECTION_EXPANDED_DEFAULT,  .defaultVal = SECTION_EXPANDED_DEFAULT,  .lowerBound = FALSE,                 .upperBound = TRUE,                       },
    [CS_OPT_MEMORY_SHOW_RANGE       ] = { .type = CS_OPT_TYPE_SETTING, .name = "Show current address range",     .valNames = &gValNames_bool,          .val = TRUE,                      .defaultVal = TRUE,                      .lowerBound = FALSE,                 .upperBound = TRUE,                       },
#ifdef INCLUDE_DEBUG_MAP
    [CS_OPT_MEMORY_SHOW_SYMBOL      ] = { .type = CS_OPT_TYPE_SETTING, .name = "Show current symbol name",       .valNames = &gValNames_bool,          .val = TRUE,                      .defaultVal = TRUE,                      .lowerBound = FALSE,                 .upperBound = TRUE,                       },
#endif // INCLUDE_DEBUG_MAP
    [CS_OPT_MEMORY_AS_ASCII         ] = { .type = CS_OPT_TYPE_SETTING, .name = "Show data as ascii",             .valNames = &gValNames_bool,          .val = FALSE,                     .defaultVal = FALSE,                     .lowerBound = FALSE,                 .upperBound = TRUE,                       },
    [CS_OPT_END_MEMORY              ] = { .type = CS_OPT_TYPE_END, },
};


const enum ControlTypes cs_cont_list_memory[] = {
    CONT_DESC_SWITCH_PAGE,
    CONT_DESC_SHOW_CONTROLS,
    CONT_DESC_HIDE_CRASH_SCREEN,
#ifdef UNF
    CONT_DESC_OS_PRINT,
#endif // UNF
    CONT_DESC_CURSOR,
    CONT_DESC_JUMP_TO_ADDRESS,
    CONT_DESC_TOGGLE_ASCII,
    CONT_DESC_LIST_END,
};


#define MEMORY_NUM_SHOWN_ROWS 20


static Address sRamViewViewportIndex = 0x00000000;
static u32 sRamViewNumShownRows = MEMORY_NUM_SHOWN_ROWS;

static const char gHex[0x10] = "0123456789ABCDEF";
#ifdef UNF
static u32 sMemoryViewData[20][4];
#endif // UNF


void page_memory_init(void) {
    sRamViewViewportIndex = gSelectedAddress;
}

static void print_byte(u32 x, u32 y, Byte byte, RGBA32 color) {
    // "[XX]"
    if (cs_get_setting_val(CS_OPT_GROUP_PAGE_MEMORY, CS_OPT_MEMORY_AS_ASCII)) {
        cs_draw_glyph((x + TEXT_WIDTH(1)), y, byte, color);
    } else {
        // Faster than doing cs_print:
        cs_draw_glyph((x + TEXT_WIDTH(0)), y, gHex[byte >> 4], color);
        cs_draw_glyph((x + TEXT_WIDTH(1)), y, gHex[byte & 0xF], color);
    }
}

static void ram_viewer_print_data(u32 line, Address startAddr) {
    const _Bool memoryAsASCII = cs_get_setting_val(CS_OPT_GROUP_PAGE_MEMORY, CS_OPT_MEMORY_AS_ASCII);
    __OSThreadContext* tc = &gCrashedThread->context;
    u32 charX = (TEXT_X(SIZEOF_HEX(Address)) + 3);
    u32 charY = TEXT_Y(line);

#ifdef UNF
    bzero(&sMemoryViewData, sizeof(sMemoryViewData));
#endif // UNF

    for (u32 y = 0; y < sRamViewNumShownRows; y++) {
        Address rowAddr = (startAddr + (y * PAGE_MEMORY_STEP));

        // Row header:
        // "[XXXXXXXX]"
        cs_print(TEXT_X(0), TEXT_Y(line + y), (STR_COLOR_PREFIX STR_HEX_WORD),
            ((y % 2) ? COLOR_RGBA32_CRASH_MEMORY_ROW1 : COLOR_RGBA32_CRASH_MEMORY_ROW2), rowAddr
        );

        charX = (TEXT_X(SIZEOF_HEX(Word)) + 3);
        charY = TEXT_Y(line + y);

        for (u32 wordOffset = 0; wordOffset < 4; wordOffset++) {
            Word_4Bytes data = {
                .word = 0x00000000,
            };
            Address currAddrAligned = (rowAddr + (wordOffset * sizeof(Word)));
            _Bool valid = try_read_data(&data.word, currAddrAligned);

#ifdef UNF
            if (valid) {
                sMemoryViewData[y][wordOffset] = data.word;
            }
#endif // UNF

            charX += 2;

            for (u32 byteOffset = 0; byteOffset < sizeof(Word); byteOffset++) {
                Address currAddr = (currAddrAligned + byteOffset);

                RGBA32 textColor = ((memoryAsASCII || (byteOffset % 2)) ? COLOR_RGBA32_CRASH_MEMORY_DATA1 : COLOR_RGBA32_CRASH_MEMORY_DATA2);
                RGBA32 selectColor = COLOR_RGBA32_NONE;

                if (currAddr == gSelectedAddress) {
                    selectColor = COLOR_RGBA32_CRASH_MEMORY_SELECT;
                    textColor = RGBA32_INVERT(textColor);
                } else if (currAddr == tc->pc) {
                    selectColor = COLOR_RGBA32_CRASH_MEMORY_PC;
                }

                if (selectColor != COLOR_RGBA32_NONE) {
                    cs_draw_rect((charX - 1), (charY - 1), (TEXT_WIDTH(2) + 1), (TEXT_WIDTH(1) + 3), selectColor);
                }

                if (valid) {
                    print_byte(charX, charY, data.byte[byteOffset], textColor);
                } else {
                    cs_draw_glyph((charX + TEXT_WIDTH(1)), charY, '*', COLOR_RGBA32_CRASH_OUT_OF_BOUNDS);
                }

                charX += (TEXT_WIDTH(2) + 1);
            }
        }
    }
}

void page_memory_draw(void) {
    __OSThreadContext* tc = &gCrashedThread->context;

    sRamViewNumShownRows = MEMORY_NUM_SHOWN_ROWS;
    const _Bool showCurrentRange  = cs_get_setting_val(CS_OPT_GROUP_PAGE_MEMORY, CS_OPT_MEMORY_SHOW_RANGE);
    sRamViewNumShownRows -= showCurrentRange;
#ifdef INCLUDE_DEBUG_MAP
    const _Bool showCurrentSymbol = cs_get_setting_val(CS_OPT_GROUP_PAGE_MEMORY, CS_OPT_MEMORY_SHOW_SYMBOL);
    sRamViewNumShownRows -= showCurrentSymbol;
#endif // INCLUDE_DEBUG_MAP

    u32 line = 1;

    Address startAddr = sRamViewViewportIndex;
    Address endAddr = (startAddr + ((sRamViewNumShownRows - 1) * PAGE_MEMORY_STEP));

    if (showCurrentRange) {
        // "[XXXXXXXX] in [XXXXXXXX]-[XXXXXXXX]"
        cs_print(TEXT_X(0), TEXT_Y(line),
            (STR_COLOR_PREFIX STR_HEX_WORD" in "STR_HEX_WORD"-"STR_HEX_WORD),
            COLOR_RGBA32_WHITE, gSelectedAddress, startAddr, endAddr
        );

        line++;
    }

#ifdef INCLUDE_DEBUG_MAP
    if (showCurrentSymbol) {
        const MapSymbol* symbol = get_map_symbol(gSelectedAddress, SYMBOL_SEARCH_BACKWARD);

        if (symbol != NULL) {
            // "[symbol]"
            cs_print_symbol_name(TEXT_X(0), TEXT_Y(line), CRASH_SCREEN_NUM_CHARS_X, symbol);
        }

        line++;

    }
#endif // INCLUDE_DEBUG_MAP

    if (
        showCurrentRange
#ifdef INCLUDE_DEBUG_MAP
        || showCurrentSymbol
#endif // INCLUDE_DEBUG_MAP
    ) {
        cs_draw_divider(DIVIDER_Y(line));
    }

    u32 charX = (TEXT_X(SIZEOF_HEX(Address)) + 3);

    // Print column headers:
    for (u32 i = 0; i < (4 * sizeof(Word)); i++) {
        if ((i % 4) == 0) {
            charX += 2;
        }

        // "[XX]"
        cs_print(charX, TEXT_Y(line), (STR_COLOR_PREFIX STR_HEX_BYTE), ((i % 2) ? COLOR_RGBA32_CRASH_MEMORY_COL1 : COLOR_RGBA32_CRASH_MEMORY_COL2), i);

        charX += (TEXT_WIDTH(2) + 1);
    }

    // Veertical divider
    cs_draw_rect((TEXT_X(SIZEOF_HEX(Address)) + 2), DIVIDER_Y(line), 1, TEXT_HEIGHT(sRamViewNumShownRows + 1), COLOR_RGBA32_CRASH_DIVIDER);

    // "MEMORY"
    cs_print(TEXT_X(1), TEXT_Y(line), "MEMORY");

    line++;

    cs_draw_divider(DIVIDER_Y(line));

    ram_viewer_print_data(line, startAddr);

    u32 line2 = (line + sRamViewNumShownRows);

    cs_draw_divider(DIVIDER_Y(line2));

    u32 scrollTop = (DIVIDER_Y(line) + 1);
    u32 scrollBottom = DIVIDER_Y(line2);

    const size_t shownSection = ((sRamViewNumShownRows - 1) * PAGE_MEMORY_STEP);

    // Scroll bar:
    cs_draw_scroll_bar(
        scrollTop, scrollBottom,
        shownSection, VIRTUAL_RAM_SIZE,
        (sRamViewViewportIndex - VIRTUAL_RAM_START),
        COLOR_RGBA32_CRASH_DIVIDER, TRUE
    );

    // Scroll bar crash position marker:
    cs_draw_scroll_bar(
        scrollTop, scrollBottom,
        shownSection, VIRTUAL_RAM_SIZE,
        (tc->pc - VIRTUAL_RAM_START),
        COLOR_RGBA32_CRASH_AT, FALSE
    );

    osWritebackDCacheAll();
}

void page_memory_input(void) {
    if (gCSDirectionFlags.pressed.up) {
        // Scroll up.
        if (gSelectedAddress >= (VIRTUAL_RAM_START + PAGE_MEMORY_STEP)) {
            gSelectedAddress -= PAGE_MEMORY_STEP;
        }
    }

    if (gCSDirectionFlags.pressed.down) {
        // Scroll down.
        if (gSelectedAddress <= (VIRTUAL_RAM_END - PAGE_MEMORY_STEP)) {
            gSelectedAddress += PAGE_MEMORY_STEP;
        }
    }

    if (gCSDirectionFlags.pressed.left) {
        // Prevent wrapping.
        if (((gSelectedAddress - 1) & BITMASK(4)) != 0xF) {
            gSelectedAddress--;
        }
    }

    if (gCSDirectionFlags.pressed.right) {
        // Prevent wrapping.
        if (((gSelectedAddress + 1) & BITMASK(4)) != 0x0) {
            gSelectedAddress++;
        }
    }

    u16 buttonPressed = gCSCompositeController->buttonPressed;

    if (buttonPressed & A_BUTTON) {
        open_address_select(gSelectedAddress);
    }

    if (buttonPressed & B_BUTTON) {
        // Toggle whether the memory is printed as hex values or as ASCII chars.
        cs_inc_setting(CS_OPT_GROUP_PAGE_MEMORY, CS_OPT_MEMORY_AS_ASCII, TRUE);
    }

    sRamViewViewportIndex = cs_clamp_view_to_selection(sRamViewViewportIndex, gSelectedAddress, sRamViewNumShownRows, PAGE_MEMORY_STEP);
}

void page_memory_print(void) {
#ifdef UNF
    debug_printf("\n");

    Address startAddr = sRamViewViewportIndex;
    Address endAddr = (startAddr + ((sRamViewNumShownRows - 1) * PAGE_MEMORY_STEP));

    debug_printf("- SECTION: ["STR_HEX_WORD"-"STR_HEX_WORD"]\n", startAddr, endAddr);

    for (u32 row = 0; row < sRamViewNumShownRows; row++) {
        debug_printf("- ["STR_HEX_WORD"]:", (startAddr + (row * PAGE_MEMORY_STEP))); // Row address.

        for (u32 wordOffset = 0; wordOffset < 4; wordOffset++) {
            debug_printf(" "STR_HEX_WORD, sMemoryViewData[row][wordOffset]);
        }

        debug_printf("\n");
    }
#endif // UNF
}


struct CSPage gCSPage_memory = {
    .name         = "MEMORY VIEW",
    .initFunc     = page_memory_init,
    .drawFunc     = page_memory_draw,
    .inputFunc    = page_memory_input,
    .printFunc    = page_memory_print,
    .contList     = cs_cont_list_memory,
    .settingsList = cs_settings_group_page_memory,
    .flags = {
        .initialized = FALSE,
        .crashed     = FALSE,
    },
};
