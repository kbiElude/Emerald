/**
 *
 * Emerald (kbi/elude @2014-2015)
 *
 * This object lets the caller submit multiple line strip data sets. Each submitted data set
 * is cached in RAM and VRAM. The data sets can be modified after having been submitted, but
 * such action will cause a performance hit, as the BO data will need to be re-uploaded.
 *
 * NOTE: ES contexts are NOT supported. (due to my laziness)
 */
#ifndef VARIA_PRIMITIVE_RENDERER_H
#define VARIA_PRIMITIVE_RENDERER_H

#include "ral/ral_types.h"
#include "varia/varia_types.h"

REFCOUNT_INSERT_DECLARATIONS(varia_primitive_renderer,
                             varia_primitive_renderer);

/** TODO */
PUBLIC EMERALD_API varia_primitive_renderer_dataset_id varia_primitive_renderer_add_dataset(varia_primitive_renderer renderer,
                                                                                            ral_primitive_type       primitive_type,
                                                                                            unsigned int             n_vertices,
                                                                                            const float*             vertex_data,
                                                                                            const float*             rgb);

/** TODO */
PUBLIC EMERALD_API void varia_primitive_renderer_change_dataset_data(varia_primitive_renderer            renderer,
                                                                     varia_primitive_renderer_dataset_id dataset_id,
                                                                     unsigned int                        n_vertices,
                                                                     const float*                        vertex_data);

/** TODO. **/
PUBLIC EMERALD_API varia_primitive_renderer varia_primitive_renderer_create(ral_context               context,
                                                                            system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API bool varia_primitive_renderer_delete_dataset(varia_primitive_renderer            renderer,
                                                                varia_primitive_renderer_dataset_id dataset_id);

/** TODO */
PUBLIC EMERALD_API ral_present_task varia_primitive_renderer_get_present_task(varia_primitive_renderer             renderer,
                                                                              system_matrix4x4                     mvp,
                                                                              unsigned int                         n_dataset_ids,
                                                                              varia_primitive_renderer_dataset_id* dataset_ids);


#endif /* VARIA_PRIMITIVE_RENDERER_H */