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
#ifndef OGL_PRIMITIVE_RENDERER_H
#define OGL_PRIMITIVE_RENDERER_H

#include "ogl/ogl_types.h"
#include "ral/ral_types.h"


REFCOUNT_INSERT_DECLARATIONS(ogl_primitive_renderer,
                             ogl_primitive_renderer);

/** TODO */
PUBLIC EMERALD_API ogl_primitive_renderer_dataset_id ogl_primitive_renderer_add_dataset(ogl_primitive_renderer renderer,
                                                                                        ral_primitive_type     primitive_type,
                                                                                        unsigned int           n_vertices,
                                                                                        const float*           vertex_data,
                                                                                        const float*           rgb);

/** TODO */
PUBLIC EMERALD_API void ogl_primitive_renderer_change_dataset_data(ogl_primitive_renderer            renderer,
                                                                   ogl_primitive_renderer_dataset_id dataset_id,
                                                                   unsigned int                      n_vertices,
                                                                   const float*                      vertex_data);

/** TODO. **/
PUBLIC EMERALD_API ogl_primitive_renderer ogl_primitive_renderer_create(ogl_context               context,
                                                                        system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API bool ogl_primitive_renderer_delete_dataset(ogl_primitive_renderer            renderer,
                                                              ogl_primitive_renderer_dataset_id dataset_id);

/** TODO */
PUBLIC EMERALD_API void ogl_primitive_renderer_draw(ogl_primitive_renderer             renderer,
                                                    system_matrix4x4                   mvp,
                                                    unsigned int                       n_dataset_ids,
                                                    ogl_primitive_renderer_dataset_id* dataset_ids);


#endif /* OGL_PRIMITIVE_RENDERER_H */