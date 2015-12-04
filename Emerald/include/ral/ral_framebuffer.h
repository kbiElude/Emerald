#ifndef RAL_FRAMEBUFFER_H
#define RAL_FRAMEBUFFER_H

#include "ral/ral_types.h"


typedef enum
{
    /* settable with ral_framebuffer_set_attachment_*(); ral_texture. */
    RAL_FRAMEBUFFER_ATTACHMENT_PROPERTY_BOUND_TEXTURE_RAL,

    /* settable with ral_framebuffer_set_attachment_*(); uint32_t */
    RAL_FRAMEBUFFER_ATTACHMENT_PROPERTY_N_LAYER,

    /* settable with ral_framebuffer_set_attachment_*(); uint32_t. */
    RAL_FRAMEBUFFER_ATTACHMENT_PROPERTY_N_MIPMAP,

    /* settable with ral_framebuffer_set_attachment_*(); ral_framebuffer_attachment_texture_type. */
    RAL_FRAMEBUFFER_ATTACHMENT_PROPERTY_TEXTURE_TYPE,

} ral_framebuffer_attachment_property;

typedef enum
{
    RAL_FRAMEBUFFER_ATTACHMENT_TEXTURE_TYPE_1D,
    RAL_FRAMEBUFFER_ATTACHMENT_TEXTURE_TYPE_2D,
    RAL_FRAMEBUFFER_ATTACHMENT_TEXTURE_TYPE_3D,
    RAL_FRAMEBUFFER_ATTACHMENT_TEXTURE_TYPE_CUBE_MAP_NEGATIVE_X,
    RAL_FRAMEBUFFER_ATTACHMENT_TEXTURE_TYPE_CUBE_MAP_NEGATIVE_Y,
    RAL_FRAMEBUFFER_ATTACHMENT_TEXTURE_TYPE_CUBE_MAP_NEGATIVE_Z,
    RAL_FRAMEBUFFER_ATTACHMENT_TEXTURE_TYPE_CUBE_MAP_POSITIVE_X,
    RAL_FRAMEBUFFER_ATTACHMENT_TEXTURE_TYPE_CUBE_MAP_POSITIVE_Y,
    RAL_FRAMEBUFFER_ATTACHMENT_TEXTURE_TYPE_CUBE_MAP_POSITIVE_Z,
    RAL_FRAMEBUFFER_ATTACHMENT_TEXTURE_TYPE_LAYERED_1D,
    RAL_FRAMEBUFFER_ATTACHMENT_TEXTURE_TYPE_LAYERED_2D_ARRAY,
    RAL_FRAMEBUFFER_ATTACHMENT_TEXTURE_TYPE_LAYERED_3D,
    RAL_FRAMEBUFFER_ATTACHMENT_TEXTURE_TYPE_LAYERED_CUBE_MAP,       /* 6   layers */
    RAL_FRAMEBUFFER_ATTACHMENT_TEXTURE_TYPE_LAYERED_CUBE_MAP_ARRAY, /* 6*n layers */

    RAL_FRAMEBUFFER_ATTACHMENT_TEXTURE_TYPE_UNKNOWN
} ral_framebuffer_attachment_texture_type;

typedef enum
{
    /* indexed color attachment.
     *
     * Index is allowed to be any value in the range 0..max_color_attachments.
     **/
    RAL_FRAMEBUFFER_ATTACHMENT_TYPE_COLOR,

    /* depth-stencil attachment.
     *
     * Index must be 0.
     */
    RAL_FRAMEBUFFER_ATTACHMENT_TYPE_DEPTH_STENCIL,

    /* Always last */
    RAL_FRAMEBUFFER_ATTACHMENT_TYPE_COUNT,
    RAL_FRAMEBUFFER_ATTACHMENT_TYPE_UNKNOWN = RAL_FRAMEBUFFER_ATTACHMENT_TYPE_COUNT
} ral_framebuffer_attachment_type;

enum
{
    /* Called back whenever color attachment configuration is changed.
     *
     * arg: index of the updated color attachment
     **/
    RAL_FRAMEBUFFER_CALLBACK_ID_COLOR_ATTACHMENT_CONFIGURATION_CHANGED,

    /* Called back whenever depth attachment configuration is changed.
     *
     * arg: not used.
     **/
    RAL_FRAMEBUFFER_CALLBACK_ID_DEPTH_STENCIL_ATTACHMENT_CONFIGURATION_CHANGED,

    /* Always last */
    RAL_FRAMEBUFFER_CALLBACK_ID_COUNT
};

typedef enum
{
    /* not settable; system_callback_manager */
    RAL_FRAMEBUFFER_PROPERTY_CALLBACK_MANAGER,

} ral_framebuffer_property;


/** TODO.
 *
 *  NOTE: This function should only be used by ral_context. Use ral_context_* entry-points() to create
 *        a framebuffer instance.
 *
 **/
PUBLIC ral_framebuffer ral_framebuffer_create(ral_context               context,
                                              system_hashed_ansi_string name);

/** TODO */
PUBLIC void ral_framebuffer_get_attachment_property(ral_framebuffer                     framebuffer,
                                                    ral_framebuffer_attachment_type     attachment_type,
                                                    uint32_t                            index,
                                                    ral_framebuffer_attachment_property property,
                                                    void*                               out_result_ptr);

/** TODO */
PUBLIC void ral_framebuffer_get_property(ral_framebuffer          framebuffer,
                                         ral_framebuffer_property property,
                                         void*                    out_result_ptr);

/** TODO */
PUBLIC void ral_framebuffer_release(ral_framebuffer& framebuffer);

/** TODO */
PUBLIC bool ral_framebuffer_set_attachment_2D(ral_framebuffer                 framebuffer,
                                              ral_framebuffer_attachment_type attachment_type,
                                              uint32_t                        index,
                                              ral_texture                     texture_2d,
                                              uint32_t                        n_mipmap);

/** TODO */
PUBLIC bool ral_framebuffer_set_attachment_property(ral_framebuffer                     framebuffer,
                                                    ral_framebuffer_attachment_type     attachment_type,
                                                    uint32_t                            index,
                                                    ral_framebuffer_attachment_property property,
                                                    const void*                         data);

#endif /* RAL_FRAMEBUFFER_H */