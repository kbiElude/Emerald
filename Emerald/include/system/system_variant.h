/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#ifndef SYSTEM_VARIANT_H
#define SYSTEM_VARIANT_H

#include "system/system_types.h"


/** TODO */
PUBLIC EMERALD_API system_variant system_variant_create(__in system_variant_type);

/** TODO */
PUBLIC EMERALD_API system_variant system_variant_create_float(__in __notnull float);

/** TODO */
PUBLIC EMERALD_API system_variant system_variant_create_int(__in __notnull int value);

/** TODO */
PUBLIC EMERALD_API void system_variant_get_ansi_string(__in __notnull system_variant, __in bool, __out __notnull system_hashed_ansi_string*);

/** TODO */
PUBLIC EMERALD_API void system_variant_get_boolean(__in __notnull system_variant, __out __notnull bool*);

/** TODO */
PUBLIC EMERALD_API void system_variant_get_float(__in __notnull system_variant, __out __notnull float*);

/** TODO */
PUBLIC EMERALD_API void system_variant_get_integer(__in __notnull system_variant, __out __notnull int*);

/** TODO */
PUBLIC EMERALD_API system_variant_type system_variant_get_type(__in __notnull system_variant);

/** TODO */
PUBLIC EMERALD_API bool system_variant_is_equal(__in __notnull system_variant, __in __notnull system_variant);

/** TODO */
PUBLIC EMERALD_API void system_variant_release(__in __notnull __deallocate(mem) system_variant);

/** TODO */
PUBLIC EMERALD_API void system_variant_set(__in __notnull system_variant, __in __notnull system_variant, bool /* should_force */);

/** TODO */
PUBLIC EMERALD_API void system_variant_set_boolean(__in __notnull system_variant, __in bool);

/** TODO */
PUBLIC EMERALD_API void system_variant_set_ansi_string(__in __notnull system_variant, __in __notnull system_hashed_ansi_string, __in bool);

/** TODO */
PUBLIC EMERALD_API void system_variant_set_integer(__in __notnull system_variant, __in int, __in bool forced);

/** TODO */
PUBLIC EMERALD_API void system_variant_set_float(__in __notnull system_variant, __in float);

/** TODO */
PUBLIC EMERALD_API void system_variant_set_float_forced(__in __notnull system_variant, __in float);

/** TODO */
PUBLIC void _system_variants_init();

/** TODO */
PUBLIC void _system_variants_deinit();

#endif /* SYSTEM_VARIANT_H */
