/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#ifndef SYSTEM_MATH_VECTOR
#define SYSTEM_MATH_VECTOR

#include "dll_exports.h"
#include "system_types.h"

/** TODO */
PUBLIC EMERALD_API void system_math_vector_add3(__in_ecount(3)  const float* a,
                                                __in_ecount(3)  const float* b,
                                                __out_ecount(3)       float* result);

/** TODO */
PUBLIC EMERALD_API void system_math_vector_cross3(__in_ecount(3)  const float* a,
                                                  __in_ecount(3)  const float* b,
                                                  __out_ecount(3)       float* result);

/** TODO */
PUBLIC EMERALD_API float system_math_vector_dot3(__in_ecount(3) const float* a,
                                                 __in_ecount(3) const float* b);

/** TODO.
 *
 *  a = a - b
 **/
PUBLIC EMERALD_API void system_math_vector_minus3(__in_ecount(3) const float* a,
                                                  __in_ecount(3) const float* b,
                                                  __out_ecount(3)      float* result);

/** TODO */
PUBLIC EMERALD_API void system_math_vector_mul3(__in_ecount(3)  const float* a,
                                                __in_ecount(3)  const float* b,
                                                __out_ecount(3)       float* result);

/** TODO */
PUBLIC EMERALD_API void system_math_vector_mul3_float(__in_ecount(3)  const float* a,
                                                      __in            const float  b,
                                                      __out_ecount(3)       float* result);

/** TODO */
PUBLIC EMERALD_API void system_math_vector_normalize3(__in_ecount(3)  const float* a,
                                                      __out_ecount(3)       float* result);

#endif /* SYSTEM_RANDOMIZER */
