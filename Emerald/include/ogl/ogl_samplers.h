/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#ifndef OGL_SAMPLERS_H
#define OGL_SAMPLERS_H

#include "ogl/ogl_types.h"

/** TODO.
 *
 *  This function is not exported.
 **/
PUBLIC ogl_samplers ogl_samplers_create(__in __notnull ogl_context context);

/** TODO */
PUBLIC EMERALD_API ogl_sampler ogl_samplers_get_sampler(__in               __notnull ogl_samplers   samplers,
                                                        __in_ecount_opt(4)           const GLfloat* border_color,
                                                        __in_opt                     GLenum*        mag_filter_ptr,
                                                        __in_opt                     GLfloat*       max_lod_ptr,
                                                        __in_opt                     GLenum*        min_filter_ptr,
                                                        __in_opt                     GLfloat*       min_lod_ptr,
                                                        __in_opt                     GLenum*        texture_compare_func_ptr,
                                                        __in_opt                     GLenum*        texture_compare_mode_ptr,
                                                        __in_opt                     GLenum*        wrap_r_ptr,
                                                        __in_opt                     GLenum*        wrap_s_ptr,
                                                        __in_opt                     GLenum*        wrap_t_ptr);

/** TODO.
 *
 *  This function is not exported.
 **/
PUBLIC void ogl_samplers_release(__in __notnull ogl_samplers samplers);

#endif /* OGL_SAMPLERS_H */