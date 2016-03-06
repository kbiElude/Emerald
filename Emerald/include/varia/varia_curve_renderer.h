/**
 *
 * Emerald (kbi/elude @2014-2016)
 *
 */
#ifndef VARIA_CURVE_RENDERER_H
#define VARIA_CURVE_RENDERER_H

#include "scene/scene_types.h"
#include "ogl/ogl_types.h"
#include "varia/varia_types.h"


REFCOUNT_INSERT_DECLARATIONS(varia_curve_renderer,
                             varia_curve_renderer)


/** TODO */
PUBLIC EMERALD_API varia_curve_item_id varia_curve_renderer_add_scene_graph_node_curve(varia_curve_renderer renderer,
                                                                                       scene_graph        graph,
                                                                                       scene_graph_node   node,
                                                                                       const float*       curve_color,
                                                                                       system_time        duration,
                                                                                       unsigned int       n_samples_per_second,
                                                                                       float              view_vector_length);

/** TODO */
PUBLIC EMERALD_API varia_curve_renderer varia_curve_renderer_create(ral_context               context,
                                                                    system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API void varia_curve_renderer_delete_curve(varia_curve_renderer renderer,
                                                          varia_curve_item_id  item_id);

/** TODO */
PUBLIC EMERALD_API void varia_curve_renderer_draw(varia_curve_renderer       renderer,
                                                  unsigned int               n_item_ids,
                                                  const varia_curve_item_id* item_ids,
                                                  system_matrix4x4           mvp);

#endif /* VARIA_CURVE_RENDERER_H */