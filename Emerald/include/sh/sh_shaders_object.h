/**
 *
 * Emerald (kbi/elude @2012)
 * 
 * TODO
 *
 */
#ifndef SH_SHADERS_OBJECT_H
#define SH_SHADERS_OBJECT_H

#include "shared.h"
#include "sh/sh_types.h"

REFCOUNT_INSERT_DECLARATIONS(sh_shaders_object, sh_shaders_object)

/** TODO. Creates VP which outputs per-vertex colors calculated from input SH.
 *
 *  MVPs taken from UBO, model data from a buffer texture.
 *
 *  Does not work with elements.
 **/
PUBLIC EMERALD_API sh_shaders_object sh_shaders_object_create_per_vertex_ubo_buffertexture(__in __notnull ogl_context               context,
                                                                                           __in           sh_samples                samples,
                                                                                           __in           uint32_t                  n_vertexes_per_instance,
                                                                                           __in           uint32_t                  n_max_instances,
                                                                                           __in __notnull ogl_shader                fp_shader,
                                                                                           __in __notnull system_hashed_ansi_string name);

/** TODO */
RENDERING_CONTEXT_CALL PUBLIC EMERALD_API void sh_shaders_object_draw_per_vertex_ubo_buffertexture(__in __notnull sh_shaders_object instance,
                                                                                                   __in           uint32_t          mvp_storage_bo_id,
                                                                                                   __in           ogl_texture       object_sh_storage_tbo,
                                                                                                   __in           ogl_texture       light_sh_storage_tbo,
                                                                                                   __in           ogl_texture       vertex_data_tbo,
                                                                                                   __in           uint32_t          n_instances);

#endif /* SH_SHADERS_OBJECT_H */