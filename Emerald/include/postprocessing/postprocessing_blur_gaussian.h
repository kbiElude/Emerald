/**
 *
 * Emerald (kbi/elude @2015-2016)
 * 
 * TODO
 */
#ifndef POSTPROCESSING_BLUR_GAUSSIAN_H
#define POSTPROCESSING_BLUR_GAUSSIAN_H

#include "postprocessing/postprocessing_types.h"
#include "ral/ral_types.h"

REFCOUNT_INSERT_DECLARATIONS(postprocessing_blur_gaussian,
                             postprocessing_blur_gaussian)

typedef enum
{
    POSTPROCESSING_BLUR_GAUSSIAN_RESOLUTION_ORIGINAL,
    POSTPROCESSING_BLUR_GAUSSIAN_RESOLUTION_HALF,
    POSTPROCESSING_BLUR_GAUSSIAN_RESOLUTION_QUARTER
} postprocessing_blur_gaussian_resolution;


/** TODO */
PUBLIC postprocessing_blur_gaussian postprocessing_blur_gaussian_create(ral_context               context,
                                                                        system_hashed_ansi_string name,
                                                                        unsigned int              n_min_taps,
                                                                        unsigned int              n_max_taps);

/** TODO */
PUBLIC ral_present_task postprocessing_blur_gaussian_create_present_task(postprocessing_blur_gaussian            blur,
                                                                         unsigned int                            n_taps,
                                                                         float                                   n_iterations,
                                                                         ral_texture_view                        src_texture_view,
                                                                         postprocessing_blur_gaussian_resolution blur_resolution);

#endif /* POSTPROCESSING_BLUR_GAUSSIAN_H */