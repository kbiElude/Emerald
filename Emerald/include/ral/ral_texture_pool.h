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
    /* not settable; bool */
    RAL_TEXTURE_POOL_PROPERTY_IS_BEING_RELEASED,
} ral_texture_pool_property;

/** TODO */
PUBLIC void ral_texture_pool_attach_context(ral_texture_pool pool,
                                            ral_context      context);

/** TODO
 *
 *  @return true if a texture has been released as a part of the collection process,
 *          false otherwise.
 **/
PUBLIC bool ral_texture_pool_collect_garbage(ral_texture_pool pool);

/** TODO */
PUBLIC ral_texture_pool ral_texture_pool_create();

/** TODO */
PUBLIC void ral_texture_pool_detach_context(ral_texture_pool pool,
                                            ral_context      context);

/** TODO */
PUBLIC void ral_texture_pool_dump_status(ral_texture_pool pool);

/** Releases all textures maintained by the texture pool. Should precede the release call. */
PUBLIC void ral_texture_pool_empty(ral_texture_pool pool);

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

/** TODO */
PUBLIC bool ral_texture_pool_return_texture(ral_texture_pool pool,
                                            ral_texture      texture);

#endif /* RAL_TEXTURE_POOL_H */