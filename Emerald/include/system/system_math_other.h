/**
 *
 * Emerald (kbi/elude @2014-2015)
 *
 */
#ifndef SYSTEM_MATH_OTHER
#define SYSTEM_MATH_OTHER

#include "system_types.h"

PRIVATE inline uint32_t system_math_other_log2_uint32(const uint32_t x)
{
#if defined(_WIN32) && !defined(_WIN64)
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

/* Hermite interpolation of @param in_value, where the lower edge of the Hermite function is specified
 * by @param min_value, and its upper edge is specified by @param max_value
 */
PRIVATE inline float system_math_smoothstep(float in_value,
                                            float min_value,
                                            float max_value)
{
    float t = (in_value - min_value) / (max_value - min_value);

    if (t < 0.0f)
    {
        t = 0.0f;
    }
    else
    if (t >= 1.0f)
    {
        t = 1.0f;
    }

    return t * t * (3.0f - 2.0f * t);
}
#endif /* SYSTEM_MATH_OTHER */
