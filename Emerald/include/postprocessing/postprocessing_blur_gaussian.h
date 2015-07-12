/**
 *
 * Emerald (kbi/elude @2015)
 * 
 * TODO
 */
#ifndef POSTPROCESSING_BLUR_GAUSSIAN_H
#define POSTPROCESSING_BLUR_GAUSSIAN_H

#include "gfx/gfx_types.h"
#include "ogl/ogl_types.h"
#include "postprocessing/postprocessing_types.h"

REFCOUNT_INSERT_DECLARATIONS(postprocessing_blur_gaussian,
                             postprocessing_blur_gaussian)

typedef enum
{
    POSTPROCESSING_BLUR_GAUSSIAN_RESOLUTION_ORIGINAL,
    POSTPROCESSING_BLUR_GAUSSIAN_RESOLUTION_HALF,
    POSTPROCESSING_BLUR_GAUSSIAN_RESOLUTION_QUARTER
} postprocessing_blur_gaussian_resolution;


/** TODO */
PUBLIC RENDERING_CONTEXT_CALL EMERALD_API postprocessing_blur_gaussian postprocessing_blur_gaussian_create(ogl_context               context,
                                                                                                           system_hashed_ansi_string name,
                                                                                                           unsigned int              n_min_taps,
                                                                                                           unsigned int              n_max_taps);

/** TODO */
PUBLIC RENDERING_CONTEXT_CALL EMERALD_API void postprocessing_blur_gaussian_execute(postprocessing_blur_gaussian            blur,
                                                                                    unsigned int                            n_taps,
                                                                                    float                                   n_iterations,
                                                                                    ogl_texture                             src_texture,
                                                                                    postprocessing_blur_gaussian_resolution blur_resolution);

#endif /* POSTPROCESSING_BLUR_GAUSSIAN_H */