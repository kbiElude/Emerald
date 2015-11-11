/**
 *
 * Emerald (kbi/elude @2014-2015)
 *
 */
#ifndef SYSTEM_MATH_VECTOR
#define SYSTEM_MATH_VECTOR

#include "system_types.h"

/** TODO */
PUBLIC EMERALD_API void system_math_vector_add3(const float* a,
                                                const float* b,
                                                      float* result);

/** TODO */
PUBLIC EMERALD_API void system_math_vector_cross3(const float* a,
                                                  const float* b,
                                                        float* result);

/** TODO */
PUBLIC EMERALD_API float system_math_vector_dot3(const float* a,
                                                 const float* b);

/** TODO */
PUBLIC EMERALD_API float system_math_vector_length2(const float* a);

/** TODO */
PUBLIC EMERALD_API float system_math_vector_length3(const float* a);

/** TODO.
 *
 *  a = a - b
 **/
PUBLIC EMERALD_API void system_math_vector_minus3(const float* a,
                                                  const float* b,
                                                        float* result);

/** TODO */
PUBLIC EMERALD_API void system_math_vector_mul3(const float* a,
                                                const float* b,
                                                      float* result);

/** TODO */
PUBLIC EMERALD_API void system_math_vector_mul3_float(const float* a,
                                                      const float  b,
                                                            float* result);

/** TODO */
PUBLIC EMERALD_API void system_math_vector_normalize2(const float* a,
                                                            float* result);

/** TODO */
PUBLIC EMERALD_API void system_math_vector_normalize3(const float* a,
                                                            float* result);

/** TODO */
PUBLIC EMERALD_API void system_math_vector_normalize4_use_vec3_length(const float* a,
                                                                            float* result);

#endif /* SYSTEM_MATH_VECTOR */
