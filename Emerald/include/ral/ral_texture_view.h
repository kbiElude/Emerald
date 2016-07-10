#ifndef RAL_TEXTURE_VIEW_H
#define RAL_TEXTURE_VIEW_H

#include "ral/ral_types.h"

typedef enum
{
    /* not settable; ral_texture_aspect */
    RAL_TEXTURE_VIEW_PROPERTY_ASPECT,

    /* not settable; ral_texture_component[4] */
    RAL_TEXTURE_VIEW_PROPERTY_COMPONENT_ORDER,

    /* not settable; ral_context */
    RAL_TEXTURE_VIEW_PROPERTY_CONTEXT,

    /* not settable; ral_format */
    RAL_TEXTURE_VIEW_PROPERTY_FORMAT,

    /* not settable; uint32_t */
    RAL_TEXTURE_VIEW_PROPERTY_N_BASE_LAYER,

    /* not settable; uint32_t */
    RAL_TEXTURE_VIEW_PROPERTY_N_BASE_MIPMAP,

    /* not settable; uint32_t */
    RAL_TEXTURE_VIEW_PROPERTY_N_LAYERS,

    /* not settable; uint32_t */
    RAL_TEXTURE_VIEW_PROPERTY_N_MIPMAPS,

    /* not settable; ral_texture */
    RAL_TEXTURE_VIEW_PROPERTY_PARENT_TEXTURE,

    /* not settable; ral_texture_type */
    RAL_TEXTURE_VIEW_PROPERTY_TYPE,

} ral_texture_view_property;

/** TODO */
PUBLIC ral_texture_view ral_texture_view_create(const ral_texture_view_create_info* create_info_ptr);

/** TODO */
PUBLIC ral_texture_view_create_info ral_texture_view_get_create_info_from_texture(ral_texture texture);

/** TODO */
PUBLIC EMERALD_API bool ral_texture_view_get_mipmap_property(ral_texture_view            texture_view,
                                                             uint32_t                    n_layer,
                                                             uint32_t                    n_mipmap,
                                                             ral_texture_mipmap_property mipmap_property,
                                                             void*                       out_result_ptr);

/** TODO */
PUBLIC EMERALD_API void ral_texture_view_get_property(ral_texture_view          texture_view,
                                                      ral_texture_view_property property,
                                                      void*                     out_result_ptr);

/** TODO */
PUBLIC void ral_texture_view_release(ral_texture_view texture_view);


#endif /* RAL_TEXTURE_VIEW_H */