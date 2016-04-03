#ifndef RAL_TEXTURE_VIEW_H
#define RAL_TEXTURE_VIEW_H

#include "ral/ral_types.h"

typedef enum
{
    /* not settable; ral_texture_format */
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

    /* not settable; ral_texture_view_type */
    RAL_TEXTURE_VIEW_PROPERTY_TYPE,

} ral_texture_view_property;

typedef enum
{
    /* Compatible with 1D and 1D Array textures */
    RAL_TEXTURE_VIEW_TYPE_1D,

    /* Compatible with 1D and 1D Array textures */
    RAL_TEXTURE_VIEW_TYPE_1D_ARRAY,

    /* Compatible with 2D and 2D Array textures */
    RAL_TEXTURE_VIEW_TYPE_2D,

    /* Compatible with 2D and 2D Array textures */
    RAL_TEXTURE_VIEW_TYPE_2D_ARRAY,

    /* Compatible with 2D Multisample & 2D Multisample Array textures */
    RAL_TEXTURE_VIEW_TYPE_2D_MULTISAMPLE,

    /* Compatible with 2D Multisample & 2D Multisample Array textures */
    RAL_TEXTURE_VIEW_TYPE_2D_MULTISAMPLE_ARRAY,

    /* Compatible with 3D textures */
    RAL_TEXTURE_VIEW_TYPE_3D,

    /* Compatible with Cube Map, 2D, 2D Array and Cube Map Array textures (pending various constraints) */
    RAL_TEXTURE_VIEW_TYPE_CUBE_MAP,

    /* Compatible with Cube Map, 2D, 2D Array and Cube Map Array textures (pending various constraints) */
    RAL_TEXTURE_VIEW_TYPE_CUBE_MAP_ARRAY,

    RAL_TEXTURE_VIEW_TYPE_UNDEFINED
} ral_texture_view_type;


typedef struct ral_texture_view_create_info
{
    ral_texture_format    format;
    ral_texture           texture;
    ral_texture_view_type type;

    uint32_t n_base_layer;
    uint32_t n_base_mip;

    uint32_t n_layers;
    uint32_t n_mips;

    ral_texture_view_create_info()
    {
        format       = RAL_TEXTURE_FORMAT_UNKNOWN;
        n_base_layer = -1;
        n_base_mip   = -1;
        n_layers     = 0;
        n_mips       = 0;
        texture      = nullptr;
        type         = RAL_TEXTURE_VIEW_TYPE_UNDEFINED;
    }
} ral_texture_view_create_info;


/** TODO */
PUBLIC ral_texture_view ral_texture_view_create(const ral_texture_view_create_info* create_info_ptr);

/** TODO */
PUBLIC EMERALD_API void ral_texture_view_get_property(ral_texture_view          texture_view,
                                                      ral_texture_view_property property,
                                                      void*                     out_result_ptr);

/** TODO */
PUBLIC void ral_texture_view_release(ral_texture_view texture_view);


#endif /* RAL_TEXTURE_VIEW_H */