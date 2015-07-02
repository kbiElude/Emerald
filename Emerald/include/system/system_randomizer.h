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
PUBLIC EMERALD_API system_randomizer system_randomizer_create(__in __notnull system_hashed_ansi_string name,
                                                              __in           __int64                   seed);

/** TODO */
PUBLIC EMERALD_API __int64 system_randomizer_pull(__in __notnull system_randomizer instance);

/** TODO */
PUBLIC EMERALD_API void system_randomizer_reset(__in __notnull system_randomizer instance,
                                                __in            __int64          seed);

#endif /* SYSTEM_RANDOMIZER */
