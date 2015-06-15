/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#ifndef SYSTEM_MATH_OTHER
#define SYSTEM_MATH_OTHER

#include "system_types.h"

PRIVATE inline uint32_t log2_uint32(const uint32_t x)
{
#ifdef _WIN32
    uint32_t result;

    __asm
    {
        bsr eax, x
        mov result, eax
    };

    return result;
#else
    /* This is NOT portable !! */
    float     x_float     = (float) x;
    uint32_t& x_float_int = (uint32_t&) x_float;
    uint32_t  x_float_exp = (x_float_int >> 23) & 0xFF;
    int32_t   result      = int32_t(x_float_exp) - 127;

    return result;
#endif
}

#endif /* SYSTEM_MATH_OTHER */
