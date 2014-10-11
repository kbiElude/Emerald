/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#ifndef SYSTEM_MATH_OTHER
#define SYSTEM_MATH_OTHER

#include "dll_exports.h"
#include "system_types.h"

PRIVATE inline uint32_t log2_uint32(const uint32_t x)
{
    uint32_t result;

    __asm
    {
        bsr eax, x
        mov result, eax
    };

    return result;
}

#endif /* SYSTEM_MATH_OTHER */
