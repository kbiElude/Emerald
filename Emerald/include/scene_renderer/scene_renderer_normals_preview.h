/**
 *
 * Emerald (kbi/elude @2014-2016)
 *
 */
#ifndef SCENE_RENDERER_NORMALS_PREVIEW_H
#define SCENE_RENDERER_NORMALS_PREVIEW_H

#include "ogl/ogl_types.h"
#include "ral/ral_types.h"
#include "scene/scene_types.h"
#include "scene_renderer/scene_renderer_types.h"


/** TODO */
PUBLIC void scene_renderer_normals_preview_append_mesh(scene_renderer_normals_preview preview,
                                                       uint32_t                       mesh_id);

/** TODO.
 *
 *  Private usage only.
 **/
PUBLIC scene_renderer_normals_preview scene_renderer_normals_preview_create(ral_context    context,
                                                                            scene          scene,
                                                                            scene_renderer owner);

/** TODO. **/
PUBLIC void scene_renderer_normals_preview_release(scene_renderer_normals_preview preview);

/** TODO */
PUBLIC void scene_renderer_normals_preview_start(scene_renderer_normals_preview preview,
                                                 system_matrix4x4               vp,
                                                 ral_texture_view               color_rt,
                                                 ral_texture_view               depth_rt);

/** TODO */
PUBLIC ral_present_task scene_renderer_normals_preview_stop(scene_renderer_normals_preview preview);

#endif /* SCENE_RENDERER_NORMALS_PREVIEW_H */