/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#ifndef SYSTEM_VARIANT_H
#define SYSTEM_VARIANT_H

#include "system/system_types.h"


/** TODO */
PUBLIC EMERALD_API system_variant system_variant_create(__in __notnull system_variant_type variant_type);

/** TODO */
PUBLIC EMERALD_API system_variant system_variant_create_float(__in __notnull float value);

/** TODO */
PUBLIC EMERALD_API system_variant system_variant_create_int(__in __notnull int value);

/** TODO */
PUBLIC EMERALD_API void system_variant_get_ansi_string(__in  __notnull system_variant            variant,
                                                       __in            bool                      forced,
                                                       __out           system_hashed_ansi_string* result);

/** TODO */
PUBLIC EMERALD_API void system_variant_get_boolean(__in  __notnull system_variant variant,
                                                   __out           bool*          result);

/** TODO */
PUBLIC EMERALD_API void system_variant_get_float(__in  __notnull system_variant variant,
                                                 __out           float*         result);

/** TODO */
PUBLIC EMERALD_API void system_variant_get_integer(__in  __notnull system_variant variant,
                                                   __out           int*           result);

/** TODO */
PUBLIC EMERALD_API system_variant_type system_variant_get_type(__in __notnull system_variant variant);

/** TODO */
PUBLIC EMERALD_API bool system_variant_is_equal(__in __notnull system_variant variant_1,
                                                __in __notnull system_variant variant_2);

/** TODO */
PUBLIC EMERALD_API void system_variant_release(__in __notnull __deallocate(mem) system_variant variant);

/** TODO */
PUBLIC EMERALD_API void system_variant_set(__in __notnull system_variant dst,
                                           __in __notnull system_variant src,
                                                          bool           should_force);

/** TODO */
PUBLIC EMERALD_API void system_variant_set_ansi_string(__in __notnull system_variant            variant,
                                                       __in __notnull system_hashed_ansi_string input,
                                                       __in           bool                      forced);

/** TODO */
PUBLIC EMERALD_API void system_variant_set_boolean(__in __notnull system_variant variant,
                                                   __in           bool           value);

/** TODO */
PUBLIC EMERALD_API void system_variant_set_float(__in __notnull system_variant variant,
                                                 __in           float          value);

/** TODO */
PUBLIC EMERALD_API void system_variant_set_float_forced(__in __notnull system_variant variant,
                                                        __in           float          value);

/** TODO */
PUBLIC EMERALD_API void system_variant_set_integer(__in __notnull system_variant variant,
                                                   __in           int            value,
                                                   __in           bool           forced);

/** TODO */
PUBLIC void _system_variants_init();

/** TODO */
PUBLIC void _system_variants_deinit();

#endif /* SYSTEM_VARIANT_H */
