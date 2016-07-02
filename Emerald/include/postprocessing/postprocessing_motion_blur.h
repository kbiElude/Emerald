/**
 *
 * Emerald (kbi/elude @2015-2016)
 * 
 * TODO
 */
#ifndef POSTPROCESSING_MOTION_BLUR_H
#define POSTPROCESSING_MOTION_BLUR_H

#include "postprocessing/postprocessing_types.h"
#include "ral/ral_types.h"
#include "system/system_types.h"

REFCOUNT_INSERT_DECLARATIONS(postprocessing_motion_blur,
                             postprocessing_motion_blur)


typedef enum
{
    POSTPROCESSING_MOTION_BLUR_IMAGE_TYPE_2D             = RAL_TEXTURE_TYPE_2D,
    POSTPROCESSING_MOTION_BLUR_IMAGE_TYPE_2D_MULTISAMPLE = RAL_TEXTURE_TYPE_MULTISAMPLE_2D,

    /* TODO: Support for other image types */
} postprocessing_motion_blur_image_type;

typedef enum
{
    POSTPROCESSING_MOTION_BLUR_IMAGE_FORMAT_RG16F,
    POSTPROCESSING_MOTION_BLUR_IMAGE_FORMAT_RG32F,
    POSTPROCESSING_MOTION_BLUR_IMAGE_FORMAT_RGBA8

    /* TODO: Support for other image formats */
} postprocessing_motion_blur_image_format;

typedef enum
{
    /* Seetable, unsigned int. Default value: 8
     *
     * Maximum number of samples to take along the velocity vector per sample/texel.
     */
     POSTPROCESSING_MOTION_BLUR_PROPERTY_N_VELOCITY_SAMPLES_MAX,

} postprocessing_motion_blur_property;

/** TODO */
PUBLIC EMERALD_API postprocessing_motion_blur postprocessing_motion_blur_create(ral_context                             context,
                                                                                postprocessing_motion_blur_image_format src_dst_color_image_format,
                                                                                postprocessing_motion_blur_image_format src_velocity_image_format,
                                                                                postprocessing_motion_blur_image_type   image_type,
                                                                                system_hashed_ansi_string               name);

/** Dispatches a compute job to generate the motion blurred version of @param input_color_texture.
 *
 *  NOTE: After calling this function but before you sample @param output_texture, a relevant memory barrier must be issued!
 *
 *  @param motion_blur                 Motion blur post-processor instance
 *  @param input_color_texture_view    Texture, whose mipmap at level POSTPROCESSING_MOTION_BLUR_PROPERTY_SRC_COLOR_IMAGE_MIPMAP_LEVEL
 *                                     for layer POSTPROCESSING_MOTION_BLUR_PROPERTY_SRC_COLOR_IMAGE_LAYER (for cases where it applies)
 *                                     will be blurred according to velocity vectors stored in @param input_velocity_texture.
 *  @param input_velocity_texture_view Velocity texture to use for the post-processing process.
 *  @param output_texture_view         Tells where to store the result data.
 *
 */
PUBLIC EMERALD_API ral_present_task postprocessing_motion_blur_get_present_task(postprocessing_blur_poisson motion_blur,
                                                                                ral_texture_view            input_color_texture_view,
                                                                                ral_texture_view            input_velocity_texture_view,
                                                                                ral_texture_view            output_texture_view);

/** TODO */
PUBLIC EMERALD_API void postprocessing_motion_blur_get_property(postprocessing_motion_blur          motion_blur,
                                                                postprocessing_motion_blur_property property,
                                                                void*                               out_value_ptr);

/** TODO */
PUBLIC EMERALD_API void postprocessing_motion_blur_set_property(postprocessing_motion_blur          motion_blur,
                                                                postprocessing_motion_blur_property property,
                                                                const void*                         value_ptr);

#endif /* POSTPROCESSING_MOTION_BLUR_H */