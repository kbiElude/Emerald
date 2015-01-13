/**
 *
 * Emerald (kbi/elude @2015)
 *
 * Implements a very simple light preview renderer. If ever necessary,
 * please re-write the implementation to make use of ogl_primitive_renderer
 * after a certain number of lights is exceeded.
 */
#ifndef OGL_SCENE_RENDERER_LIGHTS_PREVIEW_H
#define OGL_SCENE_RENDERER_LIGHTS_PREVIEW_H

#include "ogl/ogl_types.h"
#include "scene/scene_types.h"

DECLARE_HANDLE(ogl_scene_renderer_lights_preview);


/** Creates a single of light preview renderer.
 *
 *  For ogl_scene_renderer usage only.
 *
 *  @param context Rendering context to use.
*                  Context IS NOT retained by the object.
 *  @param scene   Scene that the renderer will be used for. The scene is only used to spawn the renderer programs.
 *                 Scene IS retained by the object.
 *
 *  @return Requested object or NULL if failure occured for whatever reason.
 **/
PUBLIC ogl_scene_renderer_lights_preview ogl_scene_renderer_lights_preview_create(__in __notnull ogl_context context,
                                                                                  __in __notnull scene       scene);

/** Renders a single light instance. Requires a former call to ogl_scene_renderer_lights_preview_start().
 *
 *  @param preview                  Light preview renderer instance. Must not be NULL.
 *  @param light_position           Light position in clip space. (XYZW). Must not be NULL.
 *  @param light_color              Light color (RGBA). Must not be NULL.
 *  @param light_pos_plus_direction Should be equal to (light_position + direction vector).
 *                                  The vector needs not be normalized.
 *                                  May be NULL (eg. for lights that do not have direction).
 **/
PUBLIC RENDERING_CONTEXT_CALL void ogl_scene_renderer_lights_preview_render(__in __notnull     ogl_scene_renderer_lights_preview preview,
                                                                            __in_ecount(4)     float*                            light_position,
                                                                            __in_ecount(3)     float*                            light_color,
                                                                            __in_ecount_opt(3) float*                            light_pos_plus_direction);

/** TODO */
PUBLIC RENDERING_CONTEXT_CALL void ogl_scene_renderer_lights_preview_start(__in __notnull ogl_scene_renderer_lights_preview preview);

/** TODO */
PUBLIC RENDERING_CONTEXT_CALL void ogl_scene_renderer_lights_preview_stop(__in __notnull ogl_scene_renderer_lights_preview preview);

/** TODO. **/
PUBLIC void ogl_scene_renderer_lights_preview_release(__in __notnull ogl_scene_renderer_lights_preview preview);

#endif /* OGL_SCENE_RENDERER_LIGHTS_PREVIEW_H */