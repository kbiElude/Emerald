/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#include "shared.h"
#include "system/system_log.h"
#include "system/system_math_vector.h"

/** TODO: SIMD, eh? */
const float epsilon = 1e-5f;

/** Please see header for description */
PUBLIC EMERALD_API void system_math_vector_add3(__in_ecount(3)  const float* a,
                                                __in_ecount(3)  const float* b,
                                                __out_ecount(3)       float* result)
{
    result[0] = a[0] + b[0];
    result[1] = a[1] + b[1];
    result[2] = a[2] + b[2];
}

/** Please see header for description */
PUBLIC EMERALD_API void system_math_vector_cross3(__in_ecount(3)  const float* a,
                                                  __in_ecount(3)  const float* b,
                                                  __out_ecount(3)       float* result)
{
    ASSERT_DEBUG_SYNC(a != result, "Illegal assumption: a == result");
    ASSERT_DEBUG_SYNC(b != result, "Illegal assumption: b == result");

    result[0] = a[1] * b[2] - a[2] * b[1];
    result[1] = a[2] * b[0] - a[0] * b[2];
    result[2] = a[0] * b[1] - a[1] * b[0];
}

/** Please see header for description */
PUBLIC EMERALD_API float system_math_vector_dot3(__in_ecount(3) const float* a,
                                                 __in_ecount(3) const float* b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

/** Please see header for description */
PUBLIC EMERALD_API void system_math_vector_minus3(__in_ecount(3)  const float* a,
                                                  __in_ecount(3)  const float* b,
                                                  __out_ecount(3)       float* result)
{
    result[0] = a[0] - b[0];
    result[1] = a[1] - b[1];
    result[2] = a[2] - b[2];
}

/** Please see header for description */
PUBLIC EMERALD_API void system_math_vector_mul3(__in_ecount(3)  const float* a,
                                                __in_ecount(3)  const float* b,
                                                __out_ecount(3)       float* result)
{
    result[0] = a[0] * b[0];
    result[1] = a[1] * b[1];
    result[2] = a[2] * b[2];
}

/** Please see header for descrpition */
PUBLIC EMERALD_API void system_math_vector_mul3_float(__in_ecount(3)  const float* a,
                                                      __in            const float  b,
                                                      __out_ecount(3)       float* result)
{
    result[0] = a[0] * b;
    result[1] = a[1] * b;
    result[2] = a[2] * b;
}

/** Please see header for description */
PUBLIC EMERALD_API void system_math_vector_normalize3(__in_ecount(3)  const float* a,
                                                      __out_ecount(3)       float* result)
{
    float len = sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);

    if (len >= epsilon)
    {
        result[0] = a[0] / len;
        result[1] = a[1] / len;
        result[2] = a[2] / len;
    }
    else
    {
        LOG_ERROR("system_math_vector_normalize3() called for a vector of length 0.");
    }
}
