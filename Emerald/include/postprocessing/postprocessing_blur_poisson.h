/**
 *
 * Emerald (kbi/elude @2012-2016)
 * 
 * Blurs input texture using Poisson disc samples
 *
 * TODO: Generation of Poisson samples, which would allow to set number of samples during instantiation.
 *
 * The implementation is reference counter-based.
 */
#ifndef POSTPROCESSING_BLUR_POISSON_H
#define POSTPROCESSING_BLUR_POISSON_H

#include "gfx/gfx_types.h"
#include "ogl/ogl_types.h"
#include "postprocessing/postprocessing_types.h"

REFCOUNT_INSERT_DECLARATIONS(postprocessing_blur_poisson,
                             postprocessing_blur_poisson)

typedef enum
{
    /* settable if POSTPROCESSING_BLUR_POISSON_BLUR_BLURRINESS_SOURCE_UNIFORM is specified as bluriness_source; float */
    POSTPROCESSING_BLUR_POISSON_PROPERTY_BLUR_STRENGTH
} postprocessing_blur_poisson_property;

/** TODO.
 *
 *  @param custom_shader_code Only used if bluriness_source is POSTPROCESSING_BLUR_POISSON_BLUR_BLURRINESS_SOURCE_CUSTOM */
PUBLIC EMERALD_API postprocessing_blur_poisson postprocessing_blur_poisson_create(ral_context                                       context,
                                                                                  system_hashed_ansi_string                         name,
                                                                                  postprocessing_blur_poisson_blur_bluriness_source bluriness_source,
                                                                                  const char*                                       custom_shader_code = NULL);

/** TODO */
PUBLIC EMERALD_API ral_present_task postprocessing_blur_poisson_get_present_task(postprocessing_blur_poisson blur_poisson,
                                                                                 ral_texture_view            input_texture_view,
                                                                                 ral_texture_view            result_texture_view);

/** TODO */
PUBLIC EMERALD_API const char* postprocessing_blur_poisson_get_tap_data_shader_code();

/** TODO */
PUBLIC EMERALD_API void postprocessing_blur_poisson_set_property(postprocessing_blur_poisson          blur_poisson,
                                                                 postprocessing_blur_poisson_property property,
                                                                 const void*                          data);

#endif /* POSTPROCESSING_BLUR_POISSON_H */