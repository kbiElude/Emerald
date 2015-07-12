/**
 *
 * Emerald (kbi/elude @2014-2015)
 *
 */
#ifndef OGL_CONTEXT_SAMPLERS_H
#define OGL_CONTEXT_SAMPLERS_H

#include "ogl/ogl_types.h"

/** TODO.
 *
 *  This function is not exported.
 **/
PUBLIC ogl_context_samplers ogl_context_samplers_create(ogl_context context);

/** TODO */
PUBLIC EMERALD_API ogl_sampler ogl_context_samplers_get_sampler(ogl_context_samplers samplers,
                                                                const GLfloat*       border_color,
                                                                const GLenum*        mag_filter_ptr,
                                                                const GLfloat*       max_lod_ptr,
                                                                const GLenum*        min_filter_ptr,
                                                                const GLfloat*       min_lod_ptr,
                                                                const GLenum*        texture_compare_func_ptr,
                                                                const GLenum*        texture_compare_mode_ptr,
                                                                const GLenum*        wrap_r_ptr,
                                                                const GLenum*        wrap_s_ptr,
                                                                const GLenum*        wrap_t_ptr);

/** TODO.
 *
 *  This function is not exported.
 **/
PUBLIC void ogl_context_samplers_release(ogl_context_samplers samplers);

#endif /* OGL_CONTEXT_SAMPLERS_H */