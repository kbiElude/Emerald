#ifndef RAL_TEXTURE_POOL_H
#define RAL_TEXTURE_POOL_H

#include "ral/ral_types.h"
#include "system/system_types.h"


typedef struct
{
    uint32_t           n_textures;
    const ral_texture* textures;
} _ral_texture_pool_callback_texture_dropped_arg;

typedef enum
{
    /* Called back whenever texture pool decides one or more texture instances are no longer needed
     * and can be safely deallocated.
     *
     * NOTE: Synchronous-only callback.
     *
     * arg: _ral_texture_pool_callback_texture_dropped_arg */
    RAL_TEXTURE_POOL_CALLBACK_ID_TEXTURE_DROPPED,

    RAL_TEXTURE_POOL_CALLBACK_ID_COUNT
} ral_texture_pool_callback_id;

typedef enum
{
    /* not settable; system_callback_manager */
    RAL_TEXTURE_POOL_PROPERTY_CALLBACK_MANAGER,

    /* not settable; uint32_t */
    RAL_TEXTURE_POOL_PROPERTY_N_ACTIVE_TEXTURES
} ral_texture_pool_property;

/** TODO */
PUBLIC bool ral_texture_pool_add(ral_texture_pool pool,
                                 ral_texture      texture);

/** TODO */
PUBLIC ral_texture_pool ral_texture_pool_create(ral_context in_context);

/** TODO */
PUBLIC bool ral_texture_pool_get(ral_texture_pool               pool,
                                 const ral_texture_create_info* texture_create_info_ptr,
                                 ral_texture*                   out_result_texture_ptr);

/** TODO */
PUBLIC void ral_texture_pool_get_property(ral_texture_pool          pool,
                                          ral_texture_pool_property property,
                                          void*                     out_result);

/** TODO */
PUBLIC void ral_texture_pool_release(ral_texture_pool pool);

#endif /* RAL_TEXTURE_POOL_H */