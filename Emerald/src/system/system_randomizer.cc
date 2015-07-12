/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "system/system_assertions.h"
#include "system/system_log.h"
#include "system/system_randomizer.h"

/** Internal type definitions */
typedef struct
{
    int64_t seed;
    int64_t u;
    int64_t v;
    int64_t w;

    REFCOUNT_INSERT_VARIABLES
} _system_randomizer;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(system_randomizer,
                               system_randomizer,
                               _system_randomizer);


/** TODO */
PRIVATE void _system_randomizer_init(_system_randomizer* data_ptr,
                                     int64_t             seed)
{
    memset(data_ptr,
           0,
           sizeof(_system_randomizer) );

    data_ptr->seed = seed;

    system_randomizer_reset((system_randomizer) data_ptr,
                            seed);
}

/** TODO */
PRIVATE void _system_randomizer_release(void* ptr)
{
    /* Nothing to do here */
}


/** Please see header for specification */
PUBLIC EMERALD_API system_randomizer system_randomizer_create(system_hashed_ansi_string name,
                                                              int64_t                   seed)
{
    _system_randomizer* randomizer_ptr = new (std::nothrow) _system_randomizer;

    ASSERT_DEBUG_SYNC(randomizer_ptr != NULL,
                      "Out of memory");

    if (randomizer_ptr != NULL)
    {
        _system_randomizer_init(randomizer_ptr,
                                seed);

        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(randomizer_ptr,
                                                       _system_randomizer_release,
                                                       OBJECT_TYPE_SYSTEM_RANDOMIZER, 
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\System Randomizers\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name)) );
    }

    return (system_randomizer) randomizer_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API int64_t system_randomizer_pull(system_randomizer instance)
{
    _system_randomizer* data_ptr = (_system_randomizer*) instance;

    data_ptr->u = data_ptr->u * 2862933555777941757LL + 7046029254386353087LL;

    data_ptr->v ^= data_ptr->v >> 17;
    data_ptr->v ^= data_ptr->v << 31; 
    data_ptr->v ^= data_ptr->v >> 8;
    data_ptr->w  = 4294957665U * (data_ptr->w & 0xffffffff) + (data_ptr->w >> 32);

    int64_t x = data_ptr->u ^ (data_ptr->u << 21);

    x ^= x >> 35; 
    x ^= x << 4;

    return (x + data_ptr->v) ^ data_ptr->w;
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_randomizer_reset(system_randomizer instance,
                                                int64_t           seed)
{
    _system_randomizer* data_ptr = (_system_randomizer*) instance;

    data_ptr->seed = seed;
    data_ptr->v    = 4101842887655102017LL;
    data_ptr->w    = 1;
    data_ptr->u    = seed ^ data_ptr->v; system_randomizer_pull(instance);
    data_ptr->v    = data_ptr->u;        system_randomizer_pull(instance);
    data_ptr->w    = data_ptr->v;        system_randomizer_pull(instance);
}
