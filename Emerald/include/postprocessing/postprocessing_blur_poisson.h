/**
 *
 * Emerald (kbi/elude @2012-2015)
 * 
 * Blurs input texture using Poisson disc samples
 *
 * TODO: Generation of Poisson samples, which would allow to set amount of samples during instantiation.
 *
 * The implementation is reference counter-based.
 */
#ifndef POSTPROCESSING_BLUR_POISSON_H
#define POSTPROCESSING_BLUR_POISSON_H

#include "gfx/gfx_types.h"
#include "ogl/ogl_types.h"
#include "postprocessing/postprocessing_types.h"

REFCOUNT_INSERT_DECLARATIONS(postprocessing_blur_poisson, postprocessing_blur_poisson)


/** TODO.
 *
 *  @param custom_shader_code Only used if bluriness_source is POSTPROCESSING_BLUR_POISSON_BLUR_BLURRINESS_SOURCE_CUSTOM */
PUBLIC EMERALD_API postprocessing_blur_poisson postprocessing_blur_poisson_create(ogl_context                                       context,
                                                                                  system_hashed_ansi_string                         name,
                                                                                  postprocessing_blur_poisson_blur_bluriness_source bluriness_source,
                                                                                  const char*                                       custom_shader_code = NULL);

/** TODO */
PUBLIC EMERALD_API void postprocessing_blur_poisson_execute(postprocessing_blur_poisson blur_poisson,
                                                            ogl_texture                 input_texture,
                                                            float                       blur_strength,
                                                            ogl_texture                 result_texture);

/** TODO */
PUBLIC EMERALD_API const char* postprocessing_blur_poisson_get_tap_data_shader_code();

#endif /* POSTPROCESSING_BLUR_POISSON_H */