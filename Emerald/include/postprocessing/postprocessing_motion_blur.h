/**
 *
 * Emerald (kbi/elude @2015)
 * 
 * TODO
 */
#ifndef POSTPROCESSING_MOTION_BLUR_H
#define POSTPROCESSING_MOTION_BLUR_H

#include "ogl/ogl_types.h"
#include "postprocessing/postprocessing_types.h"
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
    /* Settable, unsigned int. Default value: 0
     *
     * Index of the destination color image layer to use for _execute() calls. Ignored for non-arrayed
     * texture dimensionalities.
     */
     POSTPROCESSING_MOTION_BLUR_PROPERTY_DST_COLOR_IMAGE_LAYER,

     /* Settable, unsigned int. Default value: 0
     *
     * Destination color image mipmap level to use for _execute() calls.
     */
    POSTPROCESSING_MOTION_BLUR_PROPERTY_DST_COLOR_IMAGE_MIPMAP_LEVEL,

    /* Seetable, unsigned int. Default value: 8
     *
     * Maximum number of samples to take along the velocity vector per sample/texel.
     */
     POSTPROCESSING_MOTION_BLUR_PROPERTY_N_VELOCITY_SAMPLES_MAX,

    /* Settable, unsigned int. Default value: 0
     *
     * Index of the source color image layer to use for _execute() calls. Ignored for non-arrayed
     * texture dimensionalities.
     */
     POSTPROCESSING_MOTION_BLUR_PROPERTY_SRC_COLOR_IMAGE_LAYER,

     /* Settable, unsigned int. Default value: 0
     *
     * Source color image mipmap level to use for _execute() calls.
     */
    POSTPROCESSING_MOTION_BLUR_PROPERTY_SRC_COLOR_IMAGE_MIPMAP_LEVEL,

    /* Settable, unsigned int. Default value: 0
     *
     * Index of the source velocity image layer to use for _execute() calls. Ignored for non-arrayed
     * texture dimensionalities.
     */
     POSTPROCESSING_MOTION_BLUR_PROPERTY_SRC_VELOCITY_IMAGE_LAYER,

    /* Settable, unsigned int. Default value: 0
     *
     * Source color image mipmap level to use for _execute() calls.
     */
    POSTPROCESSING_MOTION_BLUR_PROPERTY_SRC_VELOCITY_IMAGE_MIPMAP_LEVEL,
} postprocessing_motion_blur_property;

/** TODO */
PUBLIC EMERALD_API postprocessing_motion_blur postprocessing_motion_blur_create(ogl_context                             context,
                                                                                postprocessing_motion_blur_image_format src_dst_color_image_format,
                                                                                postprocessing_motion_blur_image_format src_velocity_image_format,
                                                                                postprocessing_motion_blur_image_type   image_type,
                                                                                system_hashed_ansi_string               name);

/** Dispatches a compute job to generate the motion blurred version of @param input_color_texture.
 *
 *  NOTE: After calling this function but before you sample @param output_texture, a relevant memory barrier must be issued!
 *
 *  @param motion_blur            Motion blur post-processor instance
 *  @param input_color_texture    Texture, whose mipmap at level POSTPROCESSING_MOTION_BLUR_PROPERTY_SRC_COLOR_IMAGE_MIPMAP_LEVEL
 *                                for layer POSTPROCESSING_MOTION_BLUR_PROPERTY_SRC_COLOR_IMAGE_LAYER (for cases where it applies)
 *                                will be blurred according to velocity vectors stored in @param input_velocity_texture.
 *  @param input_velocity_texture Velocity texture to use for the post-processing process.
 *  @param output_texture         ogl_texture instance to store the result data in.
 *
 */
PUBLIC EMERALD_API RENDERING_CONTEXT_CALL void postprocessing_motion_blur_execute(postprocessing_motion_blur motion_blur,
                                                                                  ogl_texture                input_color_texture,
                                                                                  ogl_texture                input_velocity_texture,
                                                                                  ogl_texture                output_texture);

/** TODO */
PUBLIC EMERALD_API void postprocessing_motion_blur_get_property(postprocessing_motion_blur          motion_blur,
                                                                postprocessing_motion_blur_property property,
                                                                void*                               out_value_ptr);

/** TODO */
PUBLIC EMERALD_API void postprocessing_motion_blur_set_property(postprocessing_motion_blur          motion_blur,
                                                                postprocessing_motion_blur_property property,
                                                                const void*                         value_ptr);

#endif /* POSTPROCESSING_MOTION_BLUR_H */