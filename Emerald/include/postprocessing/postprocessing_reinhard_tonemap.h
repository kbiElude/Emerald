#if 0

TODO

/**
 *
 * Emerald (kbi/elude @2012-2016)
 * 
 * TODO
 *
 * The implementation is reference counter-based.
 */
#ifndef POSTPROCESSING_REINHARD_TONEMAP_H
#define POSTPROCESSING_REINHARD_TONEMAP_H

#include "gfx/gfx_types.h"
#include "postprocessing/postprocessing_types.h"


REFCOUNT_INSERT_DECLARATIONS(postprocessing_reinhard_tonemap,
                             postprocessing_reinhard_tonemap)


/** TODO */
PUBLIC EMERALD_API postprocessing_reinhard_tonemap postprocessing_reinhard_tonemap_create(ral_context               context, 
                                                                                          system_hashed_ansi_string name,
                                                                                          bool                      use_crude_downsampled_lum_average_calculation,
                                                                                          uint32_t*                 texture_size);

/** TODO */
PUBLIC EMERALD_API ral_present_task postprocessing_reinhard_tonemap_get_present_task(postprocessing_blur_poisson motion_blur,
                                                                                     ral_texture_view            in_texture_view,
                                                                                     float                       alpha,
                                                                                     float                       white_level,
                                                                                     ral_texture_view            out_texture_view);

#endif /* POSTPROCESSING_REINHARD_TONEMAP_H */

#endif