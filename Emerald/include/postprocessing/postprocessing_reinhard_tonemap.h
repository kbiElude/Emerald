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
PUBLIC EMERALD_API postprocessing_reinhard_tonemap postprocessing_reinhard_tonemap_create(__in __notnull             ogl_context               context, 
                                                                                          __in __notnull             system_hashed_ansi_string name,
                                                                                          __in __notnull             bool                      use_crude_downsampled_lum_average_calculation,
                                                                                          __in __notnull __ecount(2) uint32_t*                 texture_size);
/** TODO */
PUBLIC EMERALD_API void postprocessing_reinhard_tonemap_execute(__in __notnull postprocessing_reinhard_tonemap tonemapper,
                                                                __in __notnull ogl_texture                     in_texture,
                                                                __in           float                           alpha,
                                                                __in           float                           white_level,
                                                                __in __notnull ogl_texture                     out_texture);
#endif /* POSTPROCESSING_REINHARD_TONEMAP_H */