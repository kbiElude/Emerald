/**
 *
 * Emerald (kbi/elude @2015)
 *
 * Internal use only.
 */
#ifndef OGL_CONTEXT_VAOS_H
#define OGL_CONTEXT_VAOS_H

#include "ogl/ogl_types.h"

/** TODO
 *
 *  This function is not exported.
 */
PUBLIC void ogl_context_vaos_add_vao(__in __notnull ogl_context_vaos vaos,
                                     __in           GLuint           gl_id,
                                     __in __notnull ogl_vao          vao);

/** TODO.
 *
 *  This function is not exported.
 **/
PUBLIC ogl_context_vaos ogl_context_vaos_create(__in __notnull ogl_context context);

/** TODO
 *
 *  This function is not exported.
 *  This function should ONLY be called by ogl_vao instances.
 *
 **/
PUBLIC void ogl_context_vaos_delete_vao(__in __notnull ogl_context_vaos vaos,
                                        __in           GLuint            gl_id);

/** TODO
 *
 *  This function is not exported.
 */
PUBLIC ogl_vao ogl_context_vaos_get_vao(__in __notnull ogl_context_vaos vaos,
                                        __in           GLuint           gl_id);

/** TODO.
 *
 *  This function is not exported.
 **/
PUBLIC void ogl_context_vaos_release(__in __notnull ogl_context_vaos vaos);

#endif /* OGL_CONTEXT_VAOS_H */