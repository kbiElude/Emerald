/**
 *
 * Emerald (kbi/elude @2014)
 *
 * Helps cache program object uniform setter calls. Note that these functions are private only.
 */
#ifndef OGL_CONTEXT_PO_UNIFORMS_H
#define OGL_CONTEXT_PO_UNIFORMS_H

#include "ogl/ogl_types.h"


/* Declare private handle */
DECLARE_HANDLE(ogl_context_po_uniforms);

/** TODO */
PUBLIC ogl_context_po_uniforms ogl_context_po_uniforms_create(__in __notnull ogl_context context);

/** TODO */
PUBLIC void ogl_context_po_uniforms_init(__in __notnull ogl_context_po_uniforms                   po_uniforms,
                                         __in __notnull const ogl_context_gl_entrypoints_private* entrypoints_private_ptr);

/** TODO */
PUBLIC void ogl_context_po_uniforms_release(__in __notnull __post_invalid ogl_context_po_uniforms bindings);

/** TODO
 *
 *  @param bindings  TODO
 *
 */
PUBLIC void ogl_context_po_uniforms_sync(__in __notnull ogl_context_po_uniforms bindings);

#endif /* OGL_CONTEXT_PO_UNIFORMS_H */
