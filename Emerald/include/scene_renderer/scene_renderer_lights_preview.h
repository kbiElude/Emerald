/**
 *
 * Emerald (kbi/elude @2015)
 *
 * Implements a very simple light preview renderer. If ever necessary,
 * please re-write the implementation to make use of varia_primitive_renderer
 * after a certain number of lights is exceeded.
 */
#ifndef SCENE_RENDERER_LIGHTS_PREVIEW_H
#define SCENE_RENDERER_LIGHTS_PREVIEW_H

#include "ogl/ogl_types.h"
#include "ral/ral_types.h"
#include "scene/scene_types.h"
#include "scene_renderer/scene_renderer_types.h"


/** Creates a single of light preview renderer.
 *
 *  For scene_renderer usage only.
 *
 *  @param context Rendering context to use.
*                  Context IS NOT retained by the object.
 *  @param scene   Scene that the renderer will be used for. The scene is only used to spawn the renderer programs.
 *                 Scene IS retained by the object.
 *
 *  @return Requested object or NULL if failure occured for whatever reason.
 **/
PUBLIC scene_renderer_lights_preview scene_renderer_lights_preview_create(ral_context context,
                                                                          scene       scene);

/** Renders a single light instance. Requires a former call to scene_renderer_lights_preview_start().
 *
 *  @param preview                  Light preview renderer instance. Must not be NULL.
 *  @param light_position           Light position in clip space. (XYZW). Must not be NULL.
 *  @param light_color              Light color (RGBA). Must not be NULL.
 *  @param light_pos_plus_direction Should be equal to (light_position + direction vector).
 *                                  The vector needs not be normalized.
 *                                  May be NULL (eg. for lights that do not have direction).
 **/
PUBLIC void scene_renderer_lights_preview_render(scene_renderer_lights_preview preview,
                                                 float*                        light_position,
                                                 float*                        light_color,
                                                 float*                        light_pos_plus_direction);

/** TODO */
PUBLIC void scene_renderer_lights_preview_start(scene_renderer_lights_preview preview,
                                                ral_texture_view              color_rt);

/** TODO
 *
 *  Result present task takes 1 texture view (as specified at _start() time) and outputs exactly 1 texture view
 *  with the lights rendered.
 **/
PUBLIC ral_present_task scene_renderer_lights_preview_stop(scene_renderer_lights_preview preview);

/** TODO. **/
PUBLIC void scene_renderer_lights_preview_release(scene_renderer_lights_preview preview);

#endif /* SCENE_RENDERER_LIGHTS_PREVIEW_H */