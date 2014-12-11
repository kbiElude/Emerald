/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#ifndef SYSTEM_MATH_SRGB
#define SYSTEM_MATH_SRGB

#include "dll_exports.h"
#include "system_types.h"

PRIVATE inline float convert_sRGB_to_linear(const float& input)
{
    float result;

    if (input <= 0.04045f)
    {
        result = input * (1.0f / 12.92f);
    }
    else
    {
        result = pow((input + 0.055f) * (1.0f / 1.055f), 2.4f);
    }

    return result;
}

#endif /* SYSTEM_MATH_SRGB */
