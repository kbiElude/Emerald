/**
 *
 * Emerald (kbi/elude @2014-2015)
 *
 * Helps cache program object uniform setter calls. Note that these functions are private only.
 */
#ifndef OGL_CONTEXT_PO_UNIFORMS_H
#define OGL_CONTEXT_PO_UNIFORMS_H

#include "ogl/ogl_types.h"


/* Declare private handle */
DECLARE_HANDLE(ogl_context_po_uniforms);

/** TODO */
PUBLIC ogl_context_po_uniforms ogl_context_po_uniforms_create(ogl_context context);

/** TODO */
PUBLIC void ogl_context_po_uniforms_init(ogl_context_po_uniforms                   po_uniforms,
                                         const ogl_context_gl_entrypoints_private* entrypoints_private_ptr);

/** TODO */
PUBLIC void ogl_context_po_uniforms_release(ogl_context_po_uniforms bindings);

/** TODO
 *
 *  @param bindings  TODO
 *
 */
PUBLIC void ogl_context_po_uniforms_sync(ogl_context_po_uniforms bindings);

#endif /* OGL_CONTEXT_PO_UNIFORMS_H */
