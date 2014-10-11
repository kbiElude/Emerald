/**
 *
 * Emerald (kbi/elude @2012)
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
PUBLIC EMERALD_API postprocessing_blur_poisson postprocessing_blur_poisson_create(__in __notnull ogl_context                                       context,
                                                                                  __in __notnull system_hashed_ansi_string                         name,
                                                                                  __in           postprocessing_blur_poisson_blur_bluriness_source bluriness_source,
                                                                                  __in           const char*                                       custom_shader_code = NULL);

/** TODO */
PUBLIC EMERALD_API void postprocessing_blur_poisson_execute(__in __notnull postprocessing_blur_poisson blur_poisson,
                                                            __in __notnull ogl_texture                 input_texture,
                                                            __in           float                       blur_strength,
                                                            __in __notnull ogl_texture                 result_texture);

/** TODO */
PUBLIC EMERALD_API const char* postprocessing_blur_poisson_get_tap_data_shader_code();

#endif /* POSTPROCESSING_BLUR_POISSON_H */