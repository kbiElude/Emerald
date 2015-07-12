/**
 *
 * Emerald (kbi/elude @2014-2015)
 *
 */
#include "shared.h"
#include "system/system_log.h"
#include "system/system_math_vector.h"

/** TODO: SIMD, eh? */
const float epsilon = 1e-8f;

/** Please see header for description */
PUBLIC EMERALD_API void system_math_vector_add3(const float* a,
                                                const float* b,
                                                      float* result)
{
    result[0] = a[0] + b[0];
    result[1] = a[1] + b[1];
    result[2] = a[2] + b[2];
}

/** Please see header for description */
PUBLIC EMERALD_API void system_math_vector_cross3(const float* a,
                                                  const float* b,
                                                        float* result)
{
    ASSERT_DEBUG_SYNC(a != result,
                      "Illegal assumption: a == result");
    ASSERT_DEBUG_SYNC(b != result,
                      "Illegal assumption: b == result");

    result[0] = a[1] * b[2] - a[2] * b[1];
    result[1] = a[2] * b[0] - a[0] * b[2];
    result[2] = a[0] * b[1] - a[1] * b[0];
}

/** Please see header for description */
PUBLIC EMERALD_API float system_math_vector_dot3(const float* a,
                                                 const float* b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

/* Please see header for description */
PUBLIC EMERALD_API float system_math_vector_length3(const float* a)
{
    return sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
}

/** Please see header for description */
PUBLIC EMERALD_API void system_math_vector_minus3(const float* a,
                                                  const float* b,
                                                        float* result)
{
    result[0] = a[0] - b[0];
    result[1] = a[1] - b[1];
    result[2] = a[2] - b[2];
}

/** Please see header for description */
PUBLIC EMERALD_API void system_math_vector_mul3(const float* a,
                                                const float* b,
                                                      float* result)
{
    result[0] = a[0] * b[0];
    result[1] = a[1] * b[1];
    result[2] = a[2] * b[2];
}

/** Please see header for descrpition */
PUBLIC EMERALD_API void system_math_vector_mul3_float(const float* a,
                                                      const float  b,
                                                            float* result)
{
    result[0] = a[0] * b;
    result[1] = a[1] * b;
    result[2] = a[2] * b;
}

/** Please see header for description */
PUBLIC EMERALD_API void system_math_vector_normalize3(const float* a,
                                                            float* result)
{
    float len = system_math_vector_length3(a);

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

/** TODO */
PUBLIC EMERALD_API void system_math_vector_normalize4_use_vec3_length(const float* a,
                                                                            float* result)
{
    float len = system_math_vector_length3(a);

    if (len >= epsilon)
    {
        result[0] = a[0] / len;
        result[1] = a[1] / len;
        result[2] = a[2] / len;
        result[3] = a[3] / len;
    }
    else
    {
        LOG_ERROR("system_math_vector_normalize4_use_vec3_length() called for a vector of length 0.");
    }
}