/**
 *
 * Emerald (kbi/elude @2015-2016)
 *
 * Implements a frustum preview renderer.
 */
#ifndef SCENE_RENDERER_FRUSTUM_PREVIEW_H
#define SCENE_RENDERER_FRUSTUM_PREVIEW_H

#include "ogl/ogl_types.h"
#include "scene/scene_types.h"
#include "scene_renderer/scene_renderer_types.h"


/** TODO */
PUBLIC scene_renderer_frustum_preview scene_renderer_frustum_preview_create(ral_context context,
                                                                            scene       scene);

/** TODO */
PUBLIC void scene_renderer_frustum_preview_assign_cameras(scene_renderer_frustum_preview preview,
                                                          unsigned int                   n_cameras,
                                                          scene_camera*                  cameras);

/** TODO. **/
PUBLIC void scene_renderer_frustum_preview_release(scene_renderer_frustum_preview preview);

/** TODO */
PUBLIC RENDERING_CONTEXT_CALL void scene_renderer_frustum_preview_render(scene_renderer_frustum_preview preview,
                                                                         system_time                    time,
                                                                         system_matrix4x4               vp);

#endif /* SCENE_RENDERER_FRUSTUM_PREVIEW_H */