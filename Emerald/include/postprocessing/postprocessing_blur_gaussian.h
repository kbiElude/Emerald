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
PUBLIC RENDERING_CONTEXT_CALL EMERALD_API postprocessing_blur_gaussian postprocessing_blur_gaussian_create(__in __notnull ogl_context               context,
                                                                                                           __in __notnull system_hashed_ansi_string name,
                                                                                                           __in           unsigned int              n_min_taps,
                                                                                                           __in           unsigned int              n_max_taps);

/** TODO */
PUBLIC RENDERING_CONTEXT_CALL EMERALD_API void postprocessing_blur_gaussian_execute(     __notnull postprocessing_blur_gaussian            blur,
                                                                                    __in           unsigned int                            n_taps,
                                                                                    __in           float                                   n_iterations,
                                                                                    __in __notnull ogl_texture                             src_texture,
                                                                                    __in           postprocessing_blur_gaussian_resolution blur_resolution);

#endif /* POSTPROCESSING_BLUR_GAUSSIAN_H */