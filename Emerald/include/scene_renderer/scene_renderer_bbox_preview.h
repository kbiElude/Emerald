/**
 *
 * Emerald (kbi/elude @2014-2016)
 *
 */
#ifndef SCENE_RENDERER_BBOX_PREVIEW_H
#define SCENE_RENDERER_BBOX_PREVIEW_H

#include "ral/ral_types.h"
#include "scene/scene_types.h"
#include "scene_renderer/scene_renderer_types.h"

/** TODO */
PUBLIC void scene_renderer_bbox_preview_append_mesh(scene_renderer_bbox_preview preview,
                                                    uint32_t                    mesh_id);

/** TODO.
 *
 *  Private usage only.
 **/
PUBLIC scene_renderer_bbox_preview scene_renderer_bbox_preview_create(ral_context    context,
                                                                      scene          scene,
                                                                      scene_renderer owner);

/** TODO */
PUBLIC void scene_renderer_bbox_preview_start(scene_renderer_bbox_preview preview,
                                              ral_texture_view            rendertarget,
                                              system_matrix4x4            vp);

/** Returns a new present task which encapsulates a GPU task which draws the bounding boxl.
 *
 *  The result present task takes and exposes a single IO corresponding
 *  to the texture view the bboxes are going to be rasterized to, as specified at
 *  scene_renderer_bbox_preview_start() call time.
 **/
PUBLIC ral_present_task scene_renderer_bbox_preview_stop(scene_renderer_bbox_preview preview);

/** TODO. **/
PUBLIC void scene_renderer_bbox_preview_release(scene_renderer_bbox_preview renderer);

#endif /* SCENE_RENDERER_BBOX_PREVIEW_H */