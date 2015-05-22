/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "system/system_assertions.h"
#include "system/system_constants.h"
#include "system/system_critical_section.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_linear_alloc_pin.h"
#include "system/system_log.h"
#include "system/system_read_write_mutex.h"
#include "system/system_resizable_vector.h"
#include "system/system_resource_pool.h"
#include "system/system_variant.h"

/** Internal type definitions */
typedef struct
{
    system_variant_type type : 8;

    union
    {
        system_hashed_ansi_string ansi_string_value;
        bool                      boolean_value;
        int                       integer_value;
        float                     float_value;
    };
} _system_variant_descriptor;

typedef _system_variant_descriptor* _system_variant_descriptor_ptr;

/** Internal variables */
char                    conversion_buffer[CONVERSION_BUFFER_LENGTH];
system_read_write_mutex conversion_buffer_read_write_mutex;
system_resource_pool    variants_pool = NULL;

/** Defines */
#define N_PRECACHED_ELEMENTS_PER_NODE (sizeof(void*) / sizeof(_system_variant_descriptor) )


/** Please see header for specification */
PUBLIC EMERALD_API system_variant system_variant_create(__in __notnull system_variant_type variant_type)
{
    _system_variant_descriptor_ptr result = NULL;

    ASSERT_DEBUG_SYNC(variant_type == SYSTEM_VARIANT_ANSI_STRING || variant_type == SYSTEM_VARIANT_BOOL ||
                      variant_type == SYSTEM_VARIANT_INTEGER     || variant_type == SYSTEM_VARIANT_FLOAT,
                      "Variant type (%d) unrecognized",
                      variant_type);

    result       = (_system_variant_descriptor_ptr) system_resource_pool_get_from_pool(variants_pool);
    result->type = variant_type;

    return (system_variant) result;
}

/** Please see header for specification */
PUBLIC EMERALD_API system_variant system_variant_create_float(__in __notnull float value)
{
    system_variant new_variant = system_variant_create(SYSTEM_VARIANT_FLOAT);

    system_variant_set_float(new_variant,
                             value);

    return new_variant;
}

/** Please see header for specification */
PUBLIC EMERALD_API system_variant system_variant_create_int(__in __notnull int value)
{
    system_variant new_variant = system_variant_create(SYSTEM_VARIANT_INTEGER);

    system_variant_set_integer(new_variant,
                               value,
                               false); /* forced */

    return new_variant;
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_variant_get_ansi_string(__in  __notnull system_variant            variant,
                                                       __in            bool                      forced,
                                                       __out           system_hashed_ansi_string* result)
{
    _system_variant_descriptor_ptr variant_descriptor = (_system_variant_descriptor_ptr) variant;

    ASSERT_DEBUG_SYNC(forced                                                 ||
                      variant_descriptor->type == SYSTEM_VARIANT_ANSI_STRING,
                      "Invalid variant type");
    ASSERT_DEBUG_SYNC(result != NULL,
                      "Result is null");

    if (!forced)
    {
        *result = variant_descriptor->ansi_string_value;
    }
    else
    {
        switch(variant_descriptor->type)
        {
            case SYSTEM_VARIANT_ANSI_STRING:
            {
                *result = variant_descriptor->ansi_string_value;

                break;
            }

            case SYSTEM_VARIANT_INTEGER:
            {
                char buffer[32];

                sprintf_s(buffer,
                          32,
                          "%d",
                          variant_descriptor->integer_value);

                *result = system_hashed_ansi_string_create(buffer);

                break;
            }

            case SYSTEM_VARIANT_FLOAT:
            {
                char buffer[64];

                sprintf_s(buffer,
                          64,
                          "%.4f",
                          variant_descriptor->float_value);

                *result = system_hashed_ansi_string_create(buffer);

                break;
            }

            case SYSTEM_VARIANT_BOOL:
            {
                if (variant_descriptor->boolean_value)
                {
                    *result = system_hashed_ansi_string_create("1");
                }
                else
                {
                    *result = system_hashed_ansi_string_create("0");
                }

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false, "Unallowed conversion requested.");
            }
        } /* switch(variant_descriptor->type) */
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_variant_get_boolean(__in  __notnull system_variant variant,
                                                   __out           bool*          result)
{
    _system_variant_descriptor_ptr variant_descriptor = (_system_variant_descriptor_ptr) variant;

    ASSERT_DEBUG_SYNC(result != NULL,
                      "Result is null");

    switch(variant_descriptor->type)
    {
        case SYSTEM_VARIANT_INTEGER:
        {
            *result = (bool) (variant_descriptor->integer_value != 0);

            break;
        }

        case SYSTEM_VARIANT_FLOAT:
        {
            *result = (bool) (variant_descriptor->float_value != 0.0f);

            break;
        }

        case SYSTEM_VARIANT_BOOL:
        {
            *result = variant_descriptor->boolean_value;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unallowed conversion requested.");
        }
    } /* switch(variant_descriptor->type) */
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_variant_get_float(__in  __notnull system_variant variant,
                                                 __out           float*         result)
{
    _system_variant_descriptor_ptr variant_descriptor = (_system_variant_descriptor_ptr) variant;

    ASSERT_DEBUG_SYNC(result != NULL,
                      "Result is null");

    switch(variant_descriptor->type)
    {
        case SYSTEM_VARIANT_INTEGER:
        {
            *result = (float) variant_descriptor->integer_value;

            break;
        }

        case SYSTEM_VARIANT_FLOAT:
        {
            *result = variant_descriptor->float_value;

            break;
        }

        case SYSTEM_VARIANT_BOOL:
        {
            *result = (float) variant_descriptor->boolean_value;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false, "Unallowed conversion requested.");
        }
    } /* switch(variant_descriptor->type) */
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_variant_get_integer(__in  __notnull system_variant variant,
                                                   __out           int*           result)
{
    _system_variant_descriptor_ptr variant_descriptor = (_system_variant_descriptor_ptr) variant;

    ASSERT_DEBUG_SYNC(result != NULL,
                      "Result is null");

    switch(variant_descriptor->type)
    {
        case SYSTEM_VARIANT_INTEGER:
        {
            *result = variant_descriptor->integer_value;

            break;
        }

        case SYSTEM_VARIANT_FLOAT:
        {
            *result = (int) variant_descriptor->float_value;

            break;
        }

        case SYSTEM_VARIANT_BOOL:
        {
            *result = (int) variant_descriptor->boolean_value;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unallowed conversion requested.");
        }
    } /* switch(variant_descriptor->type) */
}

/** Please see header for specification */
PUBLIC EMERALD_API system_variant_type system_variant_get_type(__in __notnull system_variant variant)
{
    return ((_system_variant_descriptor_ptr)variant)->type;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool system_variant_is_equal(__in __notnull system_variant variant_1,
                                                __in __notnull system_variant variant_2)
{
    _system_variant_descriptor_ptr variant_1_descriptor = (_system_variant_descriptor_ptr) variant_1;
    _system_variant_descriptor_ptr variant_2_descriptor = (_system_variant_descriptor_ptr) variant_2;
    bool                           result               = false;
    
    if (variant_1                  == variant_2 ||
        variant_1_descriptor->type == variant_2_descriptor->type)
    {
        result = true;
    }
    else
    {
        switch(variant_1_descriptor->type)
        {
            case SYSTEM_VARIANT_ANSI_STRING:
            {
                result = system_hashed_ansi_string_is_equal_to_hash_string(variant_1_descriptor->ansi_string_value,
                                                                           variant_2_descriptor->ansi_string_value);

                break;
            }

            case SYSTEM_VARIANT_BOOL:
            {
                result = (variant_1_descriptor->boolean_value == variant_2_descriptor->boolean_value);

                break;
            }

            case SYSTEM_VARIANT_INTEGER:
            {
                result = (variant_1_descriptor->integer_value == variant_2_descriptor->integer_value);

                break;
            }

            case SYSTEM_VARIANT_FLOAT:
            {
                result = (variant_1_descriptor->float_value == variant_2_descriptor->float_value);

                break;
            }

            default:
            {
                ASSERT_ALWAYS_SYNC(false,
                                   "Unrecognized variant type (%d)",
                                   variant_1_descriptor->type);
            }
        } /* switch(variant_1_descriptor->type) */
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_variant_release(__in __notnull __deallocate(mem) system_variant variant)
{
    system_resource_pool_return_to_pool(variants_pool,
                                        (system_resource_pool_block) variant);
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_variant_set(__in __notnull system_variant dst,
                                           __in __notnull system_variant src,
                                                          bool           should_force)
{
    _system_variant_descriptor_ptr dst_variant_descriptor = (_system_variant_descriptor_ptr) dst;
    _system_variant_descriptor_ptr src_variant_descriptor = (_system_variant_descriptor_ptr) src;

    if (!should_force                                                                 ||
         should_force && src_variant_descriptor->type == dst_variant_descriptor->type)
    {
        switch (src_variant_descriptor->type)
        {
            case SYSTEM_VARIANT_ANSI_STRING:
            {
                dst_variant_descriptor->ansi_string_value = src_variant_descriptor->ansi_string_value;

                break;
            }

            case SYSTEM_VARIANT_BOOL:
            {
                dst_variant_descriptor->boolean_value = src_variant_descriptor->boolean_value;

                break;
            }

            case SYSTEM_VARIANT_INTEGER:
            {
                dst_variant_descriptor->integer_value = src_variant_descriptor->integer_value;

                break;
            }

            case SYSTEM_VARIANT_FLOAT:
            {
                dst_variant_descriptor->float_value = src_variant_descriptor->float_value;

                break;
            }

            default:
            {
                ASSERT_ALWAYS_SYNC(false,
                                   "Unrecognized variant type (%d)",
                                   src_variant_descriptor->type);
            }
        } /* switch (src_variant_descriptor->type) */
    
        dst_variant_descriptor->type = src_variant_descriptor->type;
    }
    else
    {
        if (dst_variant_descriptor->type == SYSTEM_VARIANT_INTEGER)
        {
            switch (src_variant_descriptor->type)
            {
                case SYSTEM_VARIANT_ANSI_STRING:
                {
                    sscanf_s(system_hashed_ansi_string_get_buffer(src_variant_descriptor->ansi_string_value),
                             "%d",
                             &dst_variant_descriptor->integer_value);

                    break;
                }

                case SYSTEM_VARIANT_BOOL:
                {
                    dst_variant_descriptor->integer_value = (int) src_variant_descriptor->boolean_value;

                    break;
                }

                case SYSTEM_VARIANT_FLOAT:
                {
                    dst_variant_descriptor->integer_value = (int) src_variant_descriptor->float_value;

                    break;
                }

                default:
                {
                    ASSERT_ALWAYS_SYNC(false,
                                       "Unsupported ?=>integer conversion.");
                }
            }
        } /* if (dst_variant_descriptor->type == SYSTEM_VARIANT_INTEGER) */
        else
        if (dst_variant_descriptor->type == SYSTEM_VARIANT_FLOAT)
        {
            switch (src_variant_descriptor->type)
            {
                case SYSTEM_VARIANT_ANSI_STRING:
                {
                    sscanf_s(system_hashed_ansi_string_get_buffer(src_variant_descriptor->ansi_string_value),
                             "%f",
                             &dst_variant_descriptor->float_value);

                    break;
                }

                case SYSTEM_VARIANT_BOOL:
                {
                    dst_variant_descriptor->float_value = (float) src_variant_descriptor->boolean_value;

                    break;
                }

                case SYSTEM_VARIANT_INTEGER:
                {
                    dst_variant_descriptor->float_value = (float) src_variant_descriptor->integer_value;

                    break;
                }

                default:
                {
                    ASSERT_ALWAYS_SYNC(false,
                                       "Unsupported ?=>float conversion.");
                }
            }
        } /* if (dst_variant_descriptor->type == SYSTEM_VARIANT_FLOAT) */
        else
        if (dst_variant_descriptor->type == SYSTEM_VARIANT_ANSI_STRING)
        {
            switch(src_variant_descriptor->type)
            {
                case SYSTEM_VARIANT_BOOL:
                {
                    if (src_variant_descriptor->boolean_value)
                    {
                        dst_variant_descriptor->ansi_string_value = system_hashed_ansi_string_create("true");
                    }
                    else
                    {
                        dst_variant_descriptor->ansi_string_value = system_hashed_ansi_string_create("false");
                    }

                    break;
                }

                case SYSTEM_VARIANT_INTEGER:
                {
                    char buffer[32];

                    sprintf_s(buffer,
                              32,
                              "%d",
                              src_variant_descriptor->integer_value);

                    dst_variant_descriptor->ansi_string_value = system_hashed_ansi_string_create(buffer);
                    break;
                }

                case SYSTEM_VARIANT_FLOAT:
                {
                    char buffer[32];

                    sprintf_s(buffer,
                              32,
                              "%8.4f",
                              src_variant_descriptor->float_value);

                    dst_variant_descriptor->ansi_string_value = system_hashed_ansi_string_create(buffer);
                    break;
                }

                default:
                {
                    ASSERT_ALWAYS_SYNC(false,
                                       "Unsupported ?=>ansi string conversion.");
                }
            } /* switch(src_variant_descriptor->type) */
        } /* if (dst_variant_descriptor->type == SYSTEM_VARIANT_ANSI_STRING) */
        else
        {
            ASSERT_ALWAYS_SYNC(false,
                               "todo");
        }
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_variant_set_ansi_string(__in __notnull system_variant            variant,
                                                       __in __notnull system_hashed_ansi_string input,
                                                       __in           bool                      forced)
{
    _system_variant_descriptor_ptr variant_descriptor = (_system_variant_descriptor_ptr) variant;

    if (!forced                                                 ||
         variant_descriptor->type == SYSTEM_VARIANT_ANSI_STRING)
    {
        ASSERT_DEBUG_SYNC(variant_descriptor->type == SYSTEM_VARIANT_ANSI_STRING,
                          "Invalid variant type");
    
        variant_descriptor->ansi_string_value = input;
    }
    else
    {
        switch (variant_descriptor->type)
        {
            case SYSTEM_VARIANT_BOOL:
            {
                int tmp_value = 0;

                sscanf_s(system_hashed_ansi_string_get_buffer(input),
                         "%d",
                        &tmp_value);

                variant_descriptor->boolean_value = (tmp_value == 1);
                break;
            }

            case SYSTEM_VARIANT_FLOAT:
            {
                sscanf_s(system_hashed_ansi_string_get_buffer(input),
                         "%f",
                        &variant_descriptor->float_value);

                break;
            }

            case SYSTEM_VARIANT_INTEGER:
            {
                sscanf_s(system_hashed_ansi_string_get_buffer(input),
                         "%d",
                        &variant_descriptor->integer_value);

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized variant descriptor type [%d]",
                                  variant_descriptor->type);
            }
        } /* switch (variant_descriptor->type) */
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_variant_set_boolean(__in __notnull system_variant variant,
                                                   __in           bool           value)
{
    _system_variant_descriptor_ptr variant_descriptor = (_system_variant_descriptor_ptr) variant;

    ASSERT_DEBUG_SYNC(variant_descriptor->type == SYSTEM_VARIANT_BOOL,
                      "Invalid variant type");
    
    variant_descriptor->boolean_value = value;
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_variant_set_float(__in __notnull system_variant variant,
                                                 __in           float          value)
{
    _system_variant_descriptor_ptr variant_descriptor = (_system_variant_descriptor_ptr) variant;

    ASSERT_DEBUG_SYNC(variant_descriptor->type == SYSTEM_VARIANT_FLOAT,
                      "Invalid variant type");
    
    variant_descriptor->float_value = value;
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_variant_set_float_forced(__in __notnull system_variant variant,
                                                        __in           float          value)
{
    _system_variant_descriptor_ptr variant_descriptor = (_system_variant_descriptor_ptr) variant;

    switch (variant_descriptor->type)
    {
        case SYSTEM_VARIANT_ANSI_STRING:
        {
            system_read_write_mutex_lock(conversion_buffer_read_write_mutex,
                                         ACCESS_WRITE);
            {
                sprintf_s(conversion_buffer,
                          CONVERSION_BUFFER_LENGTH,
                          "%.4f",
                          value);

                variant_descriptor->ansi_string_value = system_hashed_ansi_string_create(conversion_buffer);
            }
            system_read_write_mutex_unlock(conversion_buffer_read_write_mutex,
                                           ACCESS_WRITE);

            break;
        }

        case SYSTEM_VARIANT_BOOL:
        {
            variant_descriptor->boolean_value = (value != 0);

            break;
        }

        case SYSTEM_VARIANT_INTEGER:
        {
            variant_descriptor->integer_value = (int) value;

            break;
        }

        case SYSTEM_VARIANT_FLOAT:
        {
            variant_descriptor->float_value = value;

            break;
        }

        default:
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Unsupported variant type [%d] encountered",
                               variant_descriptor->type);
        }
    } /* switch (variant_descriptor->type) */
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_variant_set_integer(__in __notnull system_variant variant,
                                                   __in           int            value,
                                                   __in           bool           forced)
{
    _system_variant_descriptor_ptr variant_descriptor = (_system_variant_descriptor_ptr) variant;

    if (!forced                                            ||
        variant_descriptor->type == SYSTEM_VARIANT_INTEGER)
    {
        ASSERT_DEBUG_SYNC(variant_descriptor->type == SYSTEM_VARIANT_INTEGER,
                          "Invalid variant type");
    
        variant_descriptor->integer_value = value;
    }
    else
    {
        switch (variant_descriptor->type)
        {
            case SYSTEM_VARIANT_ANSI_STRING:
            {
                system_read_write_mutex_lock(conversion_buffer_read_write_mutex,
                                             ACCESS_WRITE);
                {
                    sprintf_s(conversion_buffer,
                              CONVERSION_BUFFER_LENGTH,
                              "%d",
                              value);

                    variant_descriptor->ansi_string_value = system_hashed_ansi_string_create(conversion_buffer);
                }

                break;
            }

            case SYSTEM_VARIANT_BOOL:
            {
                variant_descriptor->boolean_value = (value == 1);

                break;
            }

            case SYSTEM_VARIANT_FLOAT:
            {
                variant_descriptor->float_value = (float) value;

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false, "Unrecognized variant descriptor type [%d]", variant_descriptor->type);
            }
        }
    }
}

/** Please see header for specification */
PUBLIC void _system_variants_init()
{
    ASSERT_DEBUG_SYNC(variants_pool == NULL,
                      "Variants pool already initialized.");

    if (variants_pool == NULL)
    {
        variants_pool                      = system_resource_pool_create(sizeof(_system_variant_descriptor),
                                                                         VARIANTS_BASE_CAPACITY,
                                                                         0,  /* init_block_fn   */
                                                                         0); /* deinit_block_fn */
        conversion_buffer_read_write_mutex = system_read_write_mutex_create();

        memset(conversion_buffer,
               0,
               CONVERSION_BUFFER_LENGTH);

        ASSERT_ALWAYS_SYNC(variants_pool != NULL,
                           "Variants resource pool could not have been created.");
        ASSERT_ALWAYS_SYNC(conversion_buffer_read_write_mutex != NULL,
                           "Could not create RW mutex for conversion buffer.");

        LOG_INFO("Variants pool initialized.");
    }
}

/** Please see header for specification */
PUBLIC void _system_variants_deinit()
{
    ASSERT_DEBUG_SYNC(variants_pool != NULL,
                      "Variants pool not initialized.");

    if (variants_pool != NULL)
    {
        system_resource_pool_release   (variants_pool);
        system_read_write_mutex_release(conversion_buffer_read_write_mutex);

        LOG_INFO("Variants pool deinitialized");
    }
}

