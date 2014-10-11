/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#ifndef OGL_SCENE_RENDERER_BBOX_PREVIEW_H
#define OGL_SCENE_RENDERER_BBOX_PREVIEW_H

#include "ogl/ogl_types.h"
#include "scene/scene_types.h"

DECLARE_HANDLE(ogl_scene_renderer_bbox_preview);


/** TODO.
 *
 *  Private usage only.
 **/
PUBLIC ogl_scene_renderer_bbox_preview ogl_scene_renderer_bbox_preview_create(__in __notnull ogl_context        context,
                                                                              __in __notnull scene              scene,
                                                                              __in __notnull ogl_scene_renderer owner);

/** TODO */
PUBLIC RENDERING_CONTEXT_CALL void ogl_scene_renderer_bbox_preview_render(__in __notnull ogl_scene_renderer_bbox_preview preview,
                                                                          __in           uint32_t                        mesh_id);

/** TODO */
PUBLIC RENDERING_CONTEXT_CALL void ogl_scene_renderer_bbox_preview_start(__in __notnull ogl_scene_renderer_bbox_preview preview,
                                                                          __in __notnull system_matrix4x4                vp);

/** TODO */
PUBLIC RENDERING_CONTEXT_CALL void ogl_scene_renderer_bbox_preview_stop(__in __notnull ogl_scene_renderer_bbox_preview preview);

/** TODO. **/
PUBLIC void ogl_scene_renderer_bbox_preview_release(__in __notnull ogl_scene_renderer_bbox_preview renderer);

#endif /* OGL_SCENE_RENDERER_BBOX_PREVIEW_H */