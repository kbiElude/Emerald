/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#ifndef OGL_CURVE_RENDERER_H
#define OGL_CURVE_RENDERER_H

#include "scene/scene_types.h"
#include "ogl/ogl_types.h"

REFCOUNT_INSERT_DECLARATIONS(ogl_curve_renderer, ogl_curve_renderer)


/** TODO */
PUBLIC EMERALD_API ogl_curve_item_id ogl_curve_renderer_add_scene_graph_node_curve(__in            __notnull ogl_curve_renderer   renderer,
                                                                                   __in            __notnull scene_graph          graph,
                                                                                   __in            __notnull scene_graph_node     node,
                                                                                   __in_ecount(4)            const float*         curve_color,
                                                                                   __in                      system_timeline_time duration,
                                                                                   __in                      unsigned int         n_samples_per_second,
                                                                                   __in                      float                view_vector_length);

/** TODO */
PUBLIC EMERALD_API ogl_curve_renderer ogl_curve_renderer_create(__in __notnull ogl_context               context,
                                                                __in __notnull system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API void ogl_curve_renderer_delete_curve(__in __notnull ogl_curve_renderer renderer,
                                                        __in           ogl_curve_item_id  item_id);

/** TODO */
PUBLIC EMERALD_API void ogl_curve_renderer_draw(__in __notnull                    ogl_curve_renderer   renderer,
                                                __in                              unsigned int         n_item_ids,
                                                __in_ecount(n_item_ids) __notnull ogl_curve_item_id*   item_ids,
                                                __in                              system_matrix4x4     mvp);

#endif /* OGL_CURVE_RENDERER_H */