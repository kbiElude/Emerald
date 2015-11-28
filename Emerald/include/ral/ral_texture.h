#ifndef RAL_TEXTURE_H
#define RAL_TEXTURE_H

#include "gfx/gfx_image.h"
#include "gfx/gfx_types.h"
#include "ral/ral_types.h"
#include "system/system_types.h"

typedef struct
{
    ral_texture                                          texture;
    uint32_t                                             n_updates;
    const ral_texture_mipmap_client_sourced_update_info* updates;

} _ral_texture_client_memory_source_update_requested_callback_arg;

typedef enum
{
    /* Notification fired whenever the app requests one or more mipmaps to be updated
     * with data coming from the client memory.
     *
     * arg: _ral_texture_client_memory_source_update_requested_callback_arg
     */
    RAL_TEXTURE_CALLBACK_ID_CLIENT_MEMORY_SOURCE_UPDATE_REQUESTED,

    /* Fired whenever the app requests mipmap generation for the specified ral_texture
     * instance.
     *
     * arg: ral_texture instance.
     */
    RAL_TEXTURE_CALLBACK_ID_MIPMAP_GENERATION_REQUESTED,

    /* Always last */
    RAL_TEXTURE_CALLBACK_ID_COUNT,
};

typedef enum
{
    /* not settable, bool */
    RAL_TEXTURE_MIPMAP_PROPERTY_CONTENTS_SET,

    /* not settable, uint32_t int */
    RAL_TEXTURE_MIPMAP_PROPERTY_DEPTH,

    /* not settable, uint32_t */
    RAL_TEXTURE_MIPMAP_PROPERTY_HEIGHT,

    /* not settable, uint32_t */
    RAL_TEXTURE_MIPMAP_PROPERTY_WIDTH,
} ral_texture_mipmap_property;

typedef enum
{
    /* not settable, system_callback_manager */
    RAL_TEXTURE_PROPERTY_CALLBACK_MANAGER,

    /* not settable, bool */
    RAL_TEXTURE_PROPERTY_FIXED_SAMPLE_LOCATIONS,

    /* not settable, system_hashed_ansi_string */
    RAL_TEXTURE_PROPERTY_FILENAME,

    /* not settable, ral_texture_format */
    RAL_TEXTURE_PROPERTY_FORMAT,

    /* not settable, system_hashed_ansi_string */
    RAL_TEXTURE_PROPERTY_NAME,

    /* not settable, uint32_t */
    RAL_TEXTURE_PROPERTY_N_LAYERS,

    /* not settable, uint32_t*/
    RAL_TEXTURE_PROPERTY_N_MIPMAPS,

    /* not settable, uint32_t */
    RAL_TEXTURE_PROPERTY_N_SAMPLES,

    /* not settable, unsigned int */
    RAL_TEXTURE_PROPERTY_TYPE,

    /* not settable, ral_texture_usage_bits */
    RAL_TEXTURE_PROPERTY_USAGE
} ral_texture_property;

REFCOUNT_INSERT_DECLARATIONS(ral_texture,
                             ral_texture);


/** TODO.
 *
 *  NOTE: Internal use only. An equivalent of this function is exposed by ral_context.
 **/
PUBLIC ral_texture ral_texture_create(system_hashed_ansi_string      name,
                                      const ral_texture_create_info* create_info_ptr);

/** TODO.
 *
 *  NOTE: Internal use only. An equivalent of this function is exposed by ral_context.
 **/
PUBLIC ral_texture ral_texture_create_from_file_name(system_hashed_ansi_string name,
                                                     system_hashed_ansi_string file_name,
                                                     ral_texture_usage_bits    usage);

/** TODO.
 *
 *  NOTE: Internal use only. An equivalent of this function is exposed by ral_context.
 **/
PUBLIC ral_texture ral_texture_create_from_gfx_image(system_hashed_ansi_string name,
                                                     gfx_image                 image,
                                                     ral_texture_usage_bits    usage);

/** TODO */
PUBLIC EMERALD_API bool ral_texture_generate_mipmaps(ral_texture texture);

/** TODO */
PUBLIC EMERALD_API bool ral_texture_get_mipmap_property(ral_texture                 texture,
                                                        uint32_t                    n_layer,
                                                        uint32_t                    n_mipmap,
                                                        ral_texture_mipmap_property mipmap_property,
                                                        void*                       out_result_ptr);

/** TODO */
PUBLIC EMERALD_API bool ral_texture_get_property(ral_texture          texture,
                                                 ral_texture_property property,
                                                 void*                out_result_ptr);

/** TODO */
PUBLIC EMERALD_API bool ral_texture_set_mipmap_data_from_client_memory(ral_texture                                          texture,
                                                                       uint32_t                                             n_updates,
                                                                       const ral_texture_mipmap_client_sourced_update_info* updates);

#endif /* RAL_TEXTURE_H */