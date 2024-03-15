#include <ultra64.h>

#include "types.h"
#include "sm64.h"

#include "crash_screen/util/memory_read.h"
#include "crash_screen/crash_controls.h"
#include "crash_screen/crash_draw.h"
#include "crash_screen/crash_main.h"
#include "crash_screen/crash_print.h"
#include "crash_screen/crash_settings.h"
#include "crash_screen/crash_pages.h"

#include "popup_address_select.h"

#include "crash_screen/pages/page_disasm.h" // For gFillBranchBuffer.


static Address sAddressSelectTarget = 0x00000000;
static s8 sAddressSelectCharIndex = 7;


// Address select draw function.
void cs_address_select_draw(void) {
    const s32 bgStartX = (JUMP_MENU_X1 - JUMP_MENU_MARGIN_X);
    const s32 bgStartY = (JUMP_MENU_Y1 - JUMP_MENU_MARGIN_Y);
    const s32 bgW = (JUMP_MENU_W + (JUMP_MENU_MARGIN_X * 2));
    const s32 bgH = (JUMP_MENU_H + (JUMP_MENU_MARGIN_Y * 2));
    cs_draw_dark_rect(
        bgStartX, bgStartY,
        bgW, bgH,
        cs_get_setting_val(CS_OPT_GROUP_GLOBAL, CS_OPT_GLOBAL_POPUP_OPACITY)
    );

    // "GO TO:"
    cs_print((SCREEN_CENTER_X - (TEXT_WIDTH(STRLEN("GO TO")) / 2)), JUMP_MENU_Y1, "GO TO:");

    Address addr = sAddressSelectTarget;
    Word data = 0;
    _Bool isValid = try_read_data(&data, addr);

    u32 addressStartX = (SCREEN_CENTER_X - (TEXT_WIDTH(SIZEOF_HEX(Address)) / 2));
    u32 addressStartY = (JUMP_MENU_Y1 + TEXT_HEIGHT(2));
    // "[XXXXXXXX]"
    cs_print(
        addressStartX, addressStartY,
        (STR_COLOR_PREFIX STR_HEX_WORD),
        (isValid ? COLOR_RGBA32_CRASH_YES : COLOR_RGBA32_CRASH_NO), addr
    );

    u32 triangleStartX = ((addressStartX + (sAddressSelectCharIndex * TEXT_WIDTH(1))) - 1);
    u32 triangleStartY = ((addressStartY - TEXT_HEIGHT(1)) + CRASH_SCREEN_CHAR_SPACING_Y);
    // Up arrow:
    cs_draw_triangle(
        triangleStartX, triangleStartY,
        TEXT_WIDTH(1), TEXT_WIDTH(1),
        COLOR_RGBA32_CRASH_SELECT_ARROW,
        CS_TRI_UP
    );
    triangleStartY += ((TEXT_WIDTH(1) + TEXT_HEIGHT(1)) - 1);
    // Down arrow:
    cs_draw_triangle(
        triangleStartX, triangleStartY,
        TEXT_WIDTH(1), TEXT_WIDTH(1),
        COLOR_RGBA32_CRASH_SELECT_ARROW,
        CS_TRI_DOWN
    );

#ifdef INCLUDE_DEBUG_MAP
    if (isValid && cs_get_setting_val(CS_OPT_GROUP_GLOBAL, CS_OPT_ADDRESS_SELECT_SYMBOL)) {
        const MapSymbol* symbol = get_map_symbol(addr, SYMBOL_SEARCH_BACKWARD);
        if (symbol != NULL) {
            // "[mapped data name]"
            cs_print_symbol_name(JUMP_MENU_X1, (JUMP_MENU_Y1 + TEXT_HEIGHT(4)), JUMP_MENU_CHARS_X, symbol);
        }
    }
#endif // INCLUDE_DEBUG_MAP

    cs_draw_outline(bgStartX, bgStartY, bgW, bgH, COLOR_RGBA32_CRASH_DIVIDER);

    osWritebackDCacheAll();
}

extern u32 sMapViewerSelectedIndex;

// Address select input functiom.
void cs_address_select_input(void) {
    s8 change = 0;
    if (gCSDirectionFlags.pressed.left ) change = -1;
    if (gCSDirectionFlags.pressed.right) change = +1;
    sAddressSelectCharIndex = ((sAddressSelectCharIndex + change) % SIZEOF_HEX(Address));

    const u32 shift = ((SIZEOF_BITS(Address) - BITS_PER_HEX) - (sAddressSelectCharIndex * BITS_PER_HEX));
    const s8 digit = GET_HEX_DIGIT(sAddressSelectTarget, shift);
    s8 new = digit;
    change = 0;

    if (gCSDirectionFlags.pressed.up  ) change = +1;
    if (gCSDirectionFlags.pressed.down) change = -1;

    if (change != 0) {
        // Wrap to virtual ram address:
        do {
            new = ((new + change) & BITMASK(BITS_PER_HEX));
        } while (SET_HEX_DIGIT(sAddressSelectTarget, new, shift) < VIRTUAL_RAM_START);
    }

    if (new != digit) {
        sAddressSelectTarget = SET_HEX_DIGIT(sAddressSelectTarget, new, shift);
    }

    u16 buttonPressed = gCSCompositeController->buttonPressed;

    if (buttonPressed & A_BUTTON) {
        // Jump to the address and close the popup.
        cs_open_popup(CS_POPUP_NONE);

        switch (gCSPageID) {
            case PAGE_STACK_TRACE:
                cs_set_page(PAGE_DISASM);
                break;
#ifdef INCLUDE_DEBUG_MAP
            case PAGE_MAP_VIEWER:;
                s32 targetIndex = get_symbol_index_from_addr_backward(sAddressSelectTarget);
                if (targetIndex != -1) {
                    if (sMapViewerSelectedIndex == (u32)targetIndex) {
                        if (is_in_code_segment(gMapSymbols[targetIndex].addr)) {
                            cs_set_page(PAGE_DISASM);
                        } else {
                            cs_set_page(PAGE_RAM_VIEWER);
                        }
                    }
                    sMapViewerSelectedIndex = targetIndex;
                }
                break;
            case PAGE_DISASM:
                if (get_symbol_index_from_addr_forward(gSelectedAddress) != get_symbol_index_from_addr_forward(sAddressSelectTarget)) {
                    gFillBranchBuffer = TRUE;
                }
                break;
#endif // INCLUDE_DEBUG_MAP
            default:
                break;
        }

        gSelectedAddress = sAddressSelectTarget;
    }

    if (buttonPressed & B_BUTTON) {
        // Close the popup without jumping.
        cs_open_popup(CS_POPUP_NONE);
    }
}

// Open the address select (jump to address) popup box.
void open_address_select(Address dest) {
    cs_open_popup(CS_POPUP_ADDRESS_SELECT);
    sAddressSelectTarget = dest;
}

struct CSPopup gCSPopup_address_select = {
    .name      = "ADDRESS SELECT",
    .initFunc  = NULL,
    .drawFunc  = cs_address_select_draw,
    .inputFunc = cs_address_select_input,
    .flags = {
        .allowPage = FALSE,
    },
};
