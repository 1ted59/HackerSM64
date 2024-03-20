#include <ultra64.h>

#include "memory_read.h"

#include "map_parser.h"

#include "segments.h"


//! TODO: Use nm map symbol data to get this info?
//! TODO: Get this to work with stuff like bhv names.
ALIGNED8 static const AddressPair sTextRegions[] = {
TEXT_REGION_SEGMENT(boot)
TEXT_REGION_SEGMENT(main)
TEXT_REGION_SEGMENT(engine)
TEXT_REGION_SEGMENT(behavior)
TEXT_REGION_SEGMENT(goddard)
#ifdef KEEP_MARIO_HEAD
TEXT_REGION_SEGMENT(libgoddard)
#endif // KEEP_MARIO_HEAD
TEXT_REGION_SEGMENT(intro)

TEXT_REGION_GROUP(group0)
TEXT_REGION_GROUP(group1)
TEXT_REGION_GROUP(group2)
TEXT_REGION_GROUP(group3)
TEXT_REGION_GROUP(group4)
TEXT_REGION_GROUP(group5)
TEXT_REGION_GROUP(group6)
TEXT_REGION_GROUP(group7)
TEXT_REGION_GROUP(group8)
TEXT_REGION_GROUP(group9)
TEXT_REGION_GROUP(group10)
TEXT_REGION_GROUP(group11)
TEXT_REGION_GROUP(group12)
TEXT_REGION_GROUP(group13)
TEXT_REGION_GROUP(group14)
TEXT_REGION_GROUP(group15)
TEXT_REGION_GROUP(group16)
TEXT_REGION_GROUP(group17)
TEXT_REGION_GROUP(common0)
TEXT_REGION_GROUP(common1)

#define STUB_LEVEL(_0, _1, _2, _3, _4, _5, _6, _7, _8)
#define DEFINE_LEVEL(_0, _1, _2, folder, _4, _5, _6, _7, _8, _9, _10) TEXT_REGION_LEVEL(folder)
#include "levels/level_defines.h"
#undef STUB_LEVEL
#undef DEFINE_LEVEL
};


size_t gNumMapSymbols = 0; // The total number of map symbols.


/**
 * @brief Initialize the map data by running a headless DMA.
 */
void map_data_init(void) {
    gNumMapSymbols = (gMapSymbolsEnd - gMapSymbols);

    Address start = (Address)_mapDataSegmentRomStart;
    Address end   = (Address)_mapDataSegmentRomEnd;
    size_t size = (end - start);

    headless_dma(start, (size_t*)(RAM_END - RAM_1MB), size);
}

/**
 * @brief Check whether the address is in a .text segment.
 *
 * @param[in] addr Address to check.
 * @return _Bool Whether the address is in a .text segment.
 */
_Bool is_in_code_segment(Address addr) {
    for (int i = 0; i < ARRAY_COUNT(sTextRegions); i++) {
        if (addr >= sTextRegions[i].start && addr < sTextRegions[i].end) {
            return TRUE;
        }
    }

    return FALSE;
}

/**
 * @brief Get the name of a map symbol.
 *
 * @param[in] symbol MapSymbol data to use for the range.
 * @return const char* Pointer to the string of the symbol's name.
 */
const char* get_map_symbol_name(const MapSymbol* symbol) {
#ifndef INCLUDE_DEBUG_MAP
    return NULL;
#endif // INCLUDE_DEBUG_MAP
    if (symbol == NULL) {
        return NULL;
    }

    return (const char*)((u32)gMapStrings + symbol->name_offset);
}

/**
 * @brief Check whether an address is within a map symbol's range.
 *
 * @param[in] addr   The address to check.
 * @param[in] symbol The MapSymbol to check.
 * @return _Bool Whether the address is within the symbol's range.
 */
ALWAYS_INLINE static _Bool addr_is_in_symbol(Address addr, const MapSymbol* symbol) {
    return ((addr >= symbol->addr) && (addr < (symbol->addr + symbol->size)));
}

/**
 * @brief Search for a symbol index starting from the beginning.
 * Some symbol ranges overlap. Use this to get the earlier symbol on an overlap.
 *
 * @param[in] addr The address to check.
 * @return s32 Index in gMapSymbols of the MapSymbol that was found. -1 if none were found.
 */
s32 get_symbol_index_from_addr_forward(Address addr) {
#ifndef INCLUDE_DEBUG_MAP
    return -1;
#endif // INCLUDE_DEBUG_MAP
    const MapSymbol* symbol = &gMapSymbols[0];

    for (size_t i = 0; i < gNumMapSymbols; i++) {
        if (symbol == NULL) {
            break;
        }

        if (addr_is_in_symbol(addr, symbol)) {
            return i;
        }

        symbol++;
    }

    return -1;
}

/**
 * @brief Search for a symbol index starting from the end.
 * Some symbol ranges overlap. Use this to get the later symbol on an overlap.
 *
 * @param[in] addr Address to check.
 * @return s32 Index in gMapSymbols of the MapSymbol that was found. -1 if none were found.
 */
s32 get_symbol_index_from_addr_backward(Address addr) {
#ifndef INCLUDE_DEBUG_MAP
    return -1;
#endif // INCLUDE_DEBUG_MAP
    const MapSymbol* symbol = &gMapSymbols[gNumMapSymbols - 1];

    for (size_t i = gNumMapSymbols; i-- > 0;) {
        if (symbol == NULL) {
            break;
        }

        if (addr_is_in_symbol(addr, symbol)) {
            return i;
        }

        symbol--;
    }

    return -1;
}

/**
 * @brief Search for a symbol index using a binary search.
 * Some symbol ranges overlap. Use this when speed matters more than getting a specific symbol on an overlap.
 *
 * @param[in] addr The address to check.
 * @return s32 Index in gMapSymbols of the MapSymbol that was found. -1 if none were found.
 */
s32 get_symbol_index_from_addr_binary(Address addr) {
    const MapSymbol* symbol = NULL;
    size_t searchStart = 0;
    size_t searchEnd = (gNumMapSymbols - 1);
    size_t index = -1;

    while (searchStart <= searchEnd) {
        index = (searchStart + ((searchEnd - searchStart) / 2)); // Get the middle position in the new search window.
        symbol = &gMapSymbols[index];

        if (addr < symbol->addr) {
            searchEnd = (index - 1);
        } else if (addr >= (symbol->addr + symbol->size)) {
            searchStart = (index + 1);
        } else {
            return index;
        }
    }

    return -1;
}

/**
 * @brief Get the MapSymbol data that the given address is in.
 *
 * @param[in] addr            The address to check.
 * @param[in] searchDirection The direction to search in. See enum SymbolSearchDirections.
 * @return const MapSymbol* Pointer to the MapSymbol data that was found. NULL if none were found.
 */
const MapSymbol* get_map_symbol(Address addr, enum SymbolSearchDirections searchDirection) {
#ifndef INCLUDE_DEBUG_MAP
    return NULL;
#endif // INCLUDE_DEBUG_MAP
    Word data = 0;
    if (!try_read_word_aligned(&data, addr)) {
        return NULL;
    }

    s32 index = -1;
    switch (searchDirection) {
        case SYMBOL_SEARCH_FORWARD:  index = get_symbol_index_from_addr_forward(addr);  break; // Get the earlier overlapping symbol.
        case SYMBOL_SEARCH_BACKWARD: index = get_symbol_index_from_addr_backward(addr); break; // Get the later overlapping symbol.
        case SYMBOL_SEARCH_BINARY:   index = get_symbol_index_from_addr_binary(addr);   break;
    }

    if (index != -1) {
        return &gMapSymbols[index];
    }

    return NULL;
}
