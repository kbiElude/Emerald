/**
 *
 * Emerald (kbi/elude @2015)
 *
 * Implements a frustum preview renderer.
 */
#ifndef OGL_SCENE_RENDERER_FRUSTUM_PREVIEW_H
#define OGL_SCENE_RENDERER_FRUSTUM_PREVIEW_H

#include "ogl/ogl_types.h"
#include "scene/scene_types.h"

DECLARE_HANDLE(ogl_scene_renderer_frustum_preview);


/** TODO */
PUBLIC ogl_scene_renderer_frustum_preview ogl_scene_renderer_frustum_preview_create(__in __notnull ogl_context context);

/** TODO */
PUBLIC void ogl_scene_renderer_frustum_preview_assign_cameras(__in __notnull ogl_scene_renderer_frustum_preview preview,
                                                              __in           unsigned int                       n_cameras,
                                                              __in __notnull scene_camera*                      cameras);

/** TODO. **/
PUBLIC void ogl_scene_renderer_frustum_preview_release(__in __notnull ogl_scene_renderer_frustum_preview preview);

/** TODO */
PUBLIC RENDERING_CONTEXT_CALL void ogl_scene_renderer_frustum_preview_render(__in __notnull ogl_scene_renderer_frustum_preview preview,
                                                                             __in           system_timeline_time               time,
                                                                             __in __notnull system_matrix4x4                   vp);

#endif /* OGL_SCENE_RENDERER_FRUSTUM_PREVIEW_H */