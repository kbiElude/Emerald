/**
 *
 * Emerald (kbi/elude @2014)
 *
 * Helps cache sampler object binding calls. Note that these functions are private only.
 */
#ifndef OGL_CONTEXT_SAMPLER_BINDINGS_H
#define OGL_CONTEXT_SAMPLER_BINDINGS_H

#include "ogl/ogl_types.h"


/* Declare private handle */
DECLARE_HANDLE(ogl_context_sampler_bindings);

/** TODO */
PUBLIC ogl_context_sampler_bindings ogl_context_sampler_bindings_create(__in __notnull ogl_context context);

/** TODO */
PUBLIC GLuint ogl_context_sampler_bindings_get_binding(__in __notnull const ogl_context_sampler_bindings bo_bindings,
                                                       __in           GLuint                             texture_unit);

/** TODO */
PUBLIC void ogl_context_sampler_bindings_init(__in __notnull ogl_context_sampler_bindings              bindings,
                                              __in __notnull const ogl_context_gl_entrypoints_private* entrypoints_private_ptr);

/** TODO */
PUBLIC void ogl_context_sampler_bindings_release(__in __notnull __post_invalid ogl_context_sampler_bindings bindings);

/** TODO */
PUBLIC void ogl_context_sampler_bindings_set_binding(__in __notnull ogl_context_sampler_bindings bindings,
                                                     __in           GLuint                       texture_unit,
                                                     __in           GLuint                       sampler);

/** TODO
 *
 *  @param bindings  TODO
 *
 */
PUBLIC void ogl_context_sampler_bindings_sync(__in __notnull ogl_context_sampler_bindings bindings);

#endif /* OGL_CONTEXT_SAMPLER_BINDINGS_H */
