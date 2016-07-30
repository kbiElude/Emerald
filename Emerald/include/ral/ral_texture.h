#ifndef RAL_TEXTURE_H
#define RAL_TEXTURE_H

#include "gfx/gfx_image.h"
#include "gfx/gfx_types.h"
#include "ral/ral_context.h"
#include "ral/ral_types.h"
#include "system/system_types.h"
#include <memory>
#include <vector>

typedef struct
{
    bool        async;
    ral_texture texture;

} _ral_texture_mipmap_generation_requested_callback_arg;

typedef struct
{
    bool                                                                         async;
    ral_texture                                                                  texture;
    std::vector<std::shared_ptr<ral_texture_mipmap_client_sourced_update_info> > updates;

} _ral_texture_client_memory_source_update_requested_callback_arg;

enum
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
     * arg: _ral_texture_mipmap_Generation_requested_callback_arg.
     */
    RAL_TEXTURE_CALLBACK_ID_MIPMAP_GENERATION_REQUESTED,

    /* Always last */
    RAL_TEXTURE_CALLBACK_ID_COUNT,
};

typedef enum
{
    /* not settable, system_callback_manager */
    RAL_TEXTURE_PROPERTY_CALLBACK_MANAGER,

    /* not settable, ral_context */
    RAL_TEXTURE_PROPERTY_CONTEXT,

    /* not settable, ral_texture_create_info. */
    RAL_TEXTURE_PROPERTY_CREATE_INFO,

    /* not settable, bool */
    RAL_TEXTURE_PROPERTY_FIXED_SAMPLE_LOCATIONS,

    /* settable, system_hashed_ansi_string */
    RAL_TEXTURE_PROPERTY_FILENAME,

    /* not settable, ral_format */
    RAL_TEXTURE_PROPERTY_FORMAT,

    /* settable, system_hashed_ansi_string */
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


/** TODO.
 *
 *  NOTE: Internal use only. An equivalent of this function is exposed by ral_context.
 **/
PUBLIC ral_texture ral_texture_create(ral_context                    context,
                                      system_hashed_ansi_string      name,
                                      const ral_texture_create_info* create_info_ptr);

/** TODO.
 *
 *  NOTE: Internal use only. An equivalent of this function is exposed by ral_context.
 **/
PUBLIC ral_texture ral_texture_create_from_file_name(ral_context                                  context,
                                                     system_hashed_ansi_string                    name,
                                                     system_hashed_ansi_string                    file_name,
                                                     ral_texture_usage_bits                       usage,
                                                     PFNRALCONTEXTNOTIFYBACKENDABOUTNEWOBJECTPROC pfn_notify_backend_about_new_object_proc,
                                                     bool                                         async);

/** TODO.
 *
 *  NOTE: Internal use only. An equivalent of this function is exposed by ral_context.
 **/
PUBLIC ral_texture ral_texture_create_from_gfx_image(ral_context                                  context,
                                                     system_hashed_ansi_string                    name,
                                                     gfx_image                                    image,
                                                     ral_texture_usage_bits                       usage,
                                                     PFNRALCONTEXTNOTIFYBACKENDABOUTNEWOBJECTPROC pfn_notify_backend_about_new_object_proc,
                                                     bool                                         async);

/** TODO */
PUBLIC EMERALD_API bool ral_texture_generate_mipmaps(ral_texture texture,
                                                     bool        async);

/** TODO */
PUBLIC EMERALD_API bool ral_texture_get_mipmap_property(ral_texture                 texture,
                                                        uint32_t                    n_layer,
                                                        uint32_t                    n_mipmap,
                                                        ral_texture_mipmap_property mipmap_property,
                                                        void*                       out_result_ptr);

/** TODO */
PUBLIC EMERALD_API bool ral_texture_get_property(const ral_texture    texture,
                                                 ral_texture_property property,
                                                 void*                out_result_ptr);

/** TODO
 *
 *  Only ral_context is allowed to call this func.
 **/
PUBLIC void ral_texture_release(ral_texture& texture);

/** TODO
 *
 *  NOTE: <updates> is a shared pointer to @param n_updates instances of ...info structure, because the update operation(s) will be
 *        executed asynchronously. In order to avoid memory leaks, make sure to assign a correct destructor to each shared pointer instance
 *        passed via the @param updates array.
 */
PUBLIC EMERALD_API bool ral_texture_set_mipmap_data_from_client_memory(ral_texture                                                     texture,
                                                                       uint32_t                                                        n_updates,
                                                                       std::shared_ptr<ral_texture_mipmap_client_sourced_update_info>* updates,
                                                                       bool                                                            async);

/** TODO */
PUBLIC void ral_texture_set_property(ral_texture          texture,
                                     ral_texture_property property,
                                     const void*          data);

#endif /* RAL_TEXTURE_H */