/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#ifndef SYSTEM_HASH64_H
#define SYSTEM_HASH64_H

#include "system/system_types.h"


/** Calculates a 64-bit hash for a raw text pointer.
 *
 *  This function is not exported.
 *
 *  @param text   Text to calculate the hash for.
 *  @param length Length of the text to calculate the hash for. The function does not check whether
 *                this value will not cause the traveller pointer to exceed text space!
 *
 *  @return Hash value.
 */
PUBLIC system_hash64 system_hash64_calculate(const char* text,
                                             uint32_t    length);

#endif /* SYSTEM_HASH64_H */