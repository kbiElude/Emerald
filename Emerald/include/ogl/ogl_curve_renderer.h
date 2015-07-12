/**
 *
 * Emerald (kbi/elude @2014-2015)
 *
 */
#ifndef OGL_CURVE_RENDERER_H
#define OGL_CURVE_RENDERER_H

#include "scene/scene_types.h"
#include "ogl/ogl_types.h"

REFCOUNT_INSERT_DECLARATIONS(ogl_curve_renderer,
                             ogl_curve_renderer)


/** TODO */
PUBLIC EMERALD_API ogl_curve_item_id ogl_curve_renderer_add_scene_graph_node_curve(ogl_curve_renderer renderer,
                                                                                   scene_graph        graph,
                                                                                   scene_graph_node   node,
                                                                                   const float*       curve_color,
                                                                                   system_time        duration,
                                                                                   unsigned int       n_samples_per_second,
                                                                                   float              view_vector_length);

/** TODO */
PUBLIC EMERALD_API ogl_curve_renderer ogl_curve_renderer_create(ogl_context               context,
                                                                system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API void ogl_curve_renderer_delete_curve(ogl_curve_renderer renderer,
                                                        ogl_curve_item_id  item_id);

/** TODO */
PUBLIC EMERALD_API void ogl_curve_renderer_draw(      ogl_curve_renderer   renderer,
                                                      unsigned int         n_item_ids,
                                                const ogl_curve_item_id*   item_ids,
                                                      system_matrix4x4     mvp);

#endif /* OGL_CURVE_RENDERER_H */