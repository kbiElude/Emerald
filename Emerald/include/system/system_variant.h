/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#ifndef SYSTEM_VARIANT_H
#define SYSTEM_VARIANT_H

#include "system/system_types.h"


/** TODO */
PUBLIC EMERALD_API system_variant system_variant_create(system_variant_type variant_type);

/** TODO */
PUBLIC EMERALD_API system_variant system_variant_create_float(float value);

/** TODO */
PUBLIC EMERALD_API system_variant system_variant_create_int(int value);

/** TODO */
PUBLIC EMERALD_API void system_variant_get_ansi_string(system_variant            variant,
                                                       bool                      forced,
                                                       system_hashed_ansi_string* result);

/** TODO */
PUBLIC EMERALD_API void system_variant_get_boolean(system_variant variant,
                                                   bool*          result);

/** TODO */
PUBLIC EMERALD_API void system_variant_get_float(system_variant variant,
                                                 float*         result);

/** TODO */
PUBLIC EMERALD_API void system_variant_get_integer(system_variant variant,
                                                   int*           result);

/** TODO */
PUBLIC EMERALD_API system_variant_type system_variant_get_type(system_variant variant);

/** TODO */
PUBLIC EMERALD_API bool system_variant_is_equal(system_variant variant_1,
                                                system_variant variant_2);

/** TODO */
PUBLIC EMERALD_API void system_variant_release(system_variant variant);

/** TODO */
PUBLIC EMERALD_API void system_variant_set(system_variant dst,
                                           system_variant src,
                                           bool           should_force);

/** TODO */
PUBLIC EMERALD_API void system_variant_set_ansi_string(system_variant            variant,
                                                       system_hashed_ansi_string input,
                                                       bool                      forced);

/** TODO */
PUBLIC EMERALD_API void system_variant_set_boolean(system_variant variant,
                                                   bool           value);

/** TODO */
PUBLIC EMERALD_API void system_variant_set_float(system_variant variant,
                                                 float          value);

/** TODO */
PUBLIC EMERALD_API void system_variant_set_float_forced(system_variant variant,
                                                        float          value);

/** TODO */
PUBLIC EMERALD_API void system_variant_set_integer(system_variant variant,
                                                   int            value,
                                                   bool           forced);

/** TODO */
PUBLIC void _system_variants_init();

/** TODO */
PUBLIC void _system_variants_deinit();

#endif /* SYSTEM_VARIANT_H */
