/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#ifndef OGL_SCENE_RENDERER_NORMALS_PREVIEW_H
#define OGL_SCENE_RENDERER_NORMALS_PREVIEW_H

#include "ogl/ogl_types.h"
#include "ral/ral_types.h"
#include "scene/scene_types.h"

DECLARE_HANDLE(ogl_scene_renderer_normals_preview);


/** TODO.
 *
 *  Private usage only.
 **/
PUBLIC ogl_scene_renderer_normals_preview ogl_scene_renderer_normals_preview_create(ral_context        context,
                                                                                    scene              scene,
                                                                                    ogl_scene_renderer owner);

/** TODO */
PUBLIC RENDERING_CONTEXT_CALL void ogl_scene_renderer_normals_preview_render(ogl_scene_renderer_normals_preview preview,
                                                                             uint32_t                           mesh_id);

/** TODO */
PUBLIC RENDERING_CONTEXT_CALL void ogl_scene_renderer_normals_preview_start(ogl_scene_renderer_normals_preview preview,
                                                                            system_matrix4x4                   vp);

/** TODO */
PUBLIC RENDERING_CONTEXT_CALL void ogl_scene_renderer_normals_preview_stop(ogl_scene_renderer_normals_preview preview);

/** TODO. **/
PUBLIC void ogl_scene_renderer_normals_preview_release(ogl_scene_renderer_normals_preview preview);

#endif /* OGL_SCENE_RENDERER_NORMALS_PREVIEW_H */