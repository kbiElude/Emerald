/**
 *
 * Emerald (kbi/elude @2014-2015)
 *
 * Various mathematical functions which do not fit elsewhere.
 */
#ifndef SYSTEM_MATH_OTHER
#define SYSTEM_MATH_OTHER

#include "system_types.h"
#include "system/system_assertions.h"
#include "system/system_math_vector.h"

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

/* Takes a 2D quad and returns a new one with rescaled diagonal.
 *
 * @param x1_y1_size             An array of 3 floats, where:
 *                               [0]: x1
 *                               [1]: y1
 *                               [2]: Edge size
 * @param scale_multiplier       Value to multiply by the input quad's diagonal length, in order
 *                               to obtain the new quad's diagonal length.
 * @param out_resized_x1_y1_size Set to 3 floats of the same meaning as with @param x1_y1_size.
 *                               The returned quad will have a diagonal of the requested length.
 **/
PRIVATE inline void system_math_other_resize_quad2d_by_diagonal(const float* x1_y1_size,
                                                                      float  scale_multiplier,
                                                                      float* out_resized_x1_y1_size)
{
    float center[] =
    {
        x1_y1_size[0] + x1_y1_size[2] * 0.5f,
        x1_y1_size[1] + x1_y1_size[2] * 0.5f
    };
    float half_diagonal[] =
    {
        center[0] - x1_y1_size[0],
        center[1] - x1_y1_size[1]
    };
    float half_diagonal_length        = system_math_vector_length2(half_diagonal);
    float half_diagonal_normalized[2];
    float new_half_diagonal_length    = half_diagonal_length * scale_multiplier;

    system_math_vector_normalize2(half_diagonal,
                                  half_diagonal_normalized);

    ASSERT_DEBUG_SYNC(fabs(half_diagonal_normalized[0] - half_diagonal_normalized[1]) <= 1e-5f,
                      "Computation error?");

    out_resized_x1_y1_size[0] = center[0] - new_half_diagonal_length * half_diagonal_normalized[0];
    out_resized_x1_y1_size[1] = center[1] - new_half_diagonal_length * half_diagonal_normalized[1];
    out_resized_x1_y1_size[2] = 2.0f      * new_half_diagonal_length * half_diagonal_normalized[0]; /* no matter if we use [0] or [1] - it's a quad */
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

    //return t * t * (3.0f - 2.0f * t);
    return t * t * t * (10.0f + t * (-15.0f + t * 6.0f) );
}
#endif /* SYSTEM_MATH_OTHER */
