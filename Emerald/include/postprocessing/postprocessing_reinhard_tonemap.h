/**
 *
 * Emerald (kbi/elude @2012-2015)
 * 
 * TODO
 *
 * The implementation is reference counter-based.
 */
#ifndef POSTPROCESSING_REINHARD_TONEMAP_H
#define POSTPROCESSING_REINHARD_TONEMAP_H

#include "gfx/gfx_types.h"
#include "ogl/ogl_types.h"
#include "postprocessing/postprocessing_types.h"


REFCOUNT_INSERT_DECLARATIONS(postprocessing_reinhard_tonemap,
                             postprocessing_reinhard_tonemap)


/** TODO */
PUBLIC EMERALD_API postprocessing_reinhard_tonemap postprocessing_reinhard_tonemap_create(ral_context               context, 
                                                                                          system_hashed_ansi_string name,
                                                                                          bool                      use_crude_downsampled_lum_average_calculation,
                                                                                          uint32_t*                 texture_size);
/** TODO */
PUBLIC EMERALD_API void postprocessing_reinhard_tonemap_execute(postprocessing_reinhard_tonemap tonemapper,
                                                                ral_texture                     in_texture,
                                                                float                           alpha,
                                                                float                           white_level,
                                                                ral_texture                     out_texture);
#endif /* POSTPROCESSING_REINHARD_TONEMAP_H */