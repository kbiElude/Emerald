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
PUBLIC ogl_context_samplers ogl_context_samplers_create(__in __notnull ogl_context context);

/** TODO */
PUBLIC EMERALD_API ogl_sampler ogl_context_samplers_get_sampler(__in               __notnull ogl_context_samplers samplers,
                                                                __in_ecount_opt(4)           const GLfloat*       border_color,
                                                                __in_opt                     const GLenum*        mag_filter_ptr,
                                                                __in_opt                     const GLfloat*       max_lod_ptr,
                                                                __in_opt                     const GLenum*        min_filter_ptr,
                                                                __in_opt                     const GLfloat*       min_lod_ptr,
                                                                __in_opt                     const GLenum*        texture_compare_func_ptr,
                                                                __in_opt                     const GLenum*        texture_compare_mode_ptr,
                                                                __in_opt                     const GLenum*        wrap_r_ptr,
                                                                __in_opt                     const GLenum*        wrap_s_ptr,
                                                                __in_opt                     const GLenum*        wrap_t_ptr);

/** TODO.
 *
 *  This function is not exported.
 **/
PUBLIC void ogl_context_samplers_release(__in __notnull ogl_context_samplers samplers);

#endif /* OGL_CONTEXT_SAMPLERS_H */