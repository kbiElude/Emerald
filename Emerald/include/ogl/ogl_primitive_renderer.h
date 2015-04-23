/**
 *
 * Emerald (kbi/elude @2014)
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

REFCOUNT_INSERT_DECLARATIONS(ogl_primitive_renderer, ogl_primitive_renderer);

/** TODO */
PUBLIC EMERALD_API ogl_primitive_renderer_dataset_id ogl_primitive_renderer_add_dataset(__in                      __notnull ogl_primitive_renderer renderer,
                                                                                        __in                                ogl_primitive_type     primitive_type,
                                                                                        __in                                unsigned int           n_vertices,
                                                                                        __in_ecount(3*n_vertices) __notnull const float*           vertex_data,
                                                                                        __in_ecount(4)            __notnull const float*           rgb);

/** TODO */
PUBLIC EMERALD_API void ogl_primitive_renderer_change_dataset_data(__in                       __notnull ogl_primitive_renderer            renderer,
                                                                    __in                                ogl_primitive_renderer_dataset_id dataset_id,
                                                                    __in                                unsigned int                      n_vertices,
                                                                    __in_ecount(3*n_vertices) __notnull const float*                      vertex_data);

/** TODO. **/
PUBLIC EMERALD_API ogl_primitive_renderer ogl_primitive_renderer_create(__in __notnull ogl_context               context,
                                                                        __in __notnull system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API bool ogl_primitive_renderer_delete_dataset(__in __notnull ogl_primitive_renderer            renderer,
                                                              __in           ogl_primitive_renderer_dataset_id dataset_id);

/** TODO */
PUBLIC EMERALD_API void ogl_primitive_renderer_draw(__in                       __notnull ogl_primitive_renderer             renderer,
                                                    __in                       __notnull system_matrix4x4                   mvp,
                                                    __in                                 unsigned int                       n_dataset_ids,
                                                    __in_ecount(n_dataset_ids) __notnull ogl_primitive_renderer_dataset_id* dataset_ids);


#endif /* OGL_PRIMITIVE_RENDERER_H */