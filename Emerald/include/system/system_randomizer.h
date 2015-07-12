/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#ifndef SYSTEM_RANDOMIZER
#define SYSTEM_RANDOMIZER

#include "system_types.h"

REFCOUNT_INSERT_DECLARATIONS(system_randomizer,
                             system_randomizer)

/** TODO */
PUBLIC EMERALD_API system_randomizer system_randomizer_create(system_hashed_ansi_string name,
                                                              int64_t                   seed);

/** TODO */
PUBLIC EMERALD_API int64_t system_randomizer_pull(system_randomizer instance);

/** TODO */
PUBLIC EMERALD_API void system_randomizer_reset(system_randomizer instance,
                                                int64_t           seed);

#endif /* SYSTEM_RANDOMIZER */
