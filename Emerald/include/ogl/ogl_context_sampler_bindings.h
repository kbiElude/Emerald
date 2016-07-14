/**
 *
 * Emerald (kbi/elude @2014-2016)
 *
 * Helps cache sampler object binding calls. Note that these functions are private only.
 */
#ifndef OGL_CONTEXT_SAMPLER_BINDINGS_H
#define OGL_CONTEXT_SAMPLER_BINDINGS_H

#include "ogl/ogl_types.h"


/* Declare private handle */
DECLARE_HANDLE(ogl_context_sampler_bindings);

/** TODO */
PUBLIC ogl_context_sampler_bindings ogl_context_sampler_bindings_create(ogl_context context);

/** TODO */
PUBLIC GLuint ogl_context_sampler_bindings_get_binding(const ogl_context_sampler_bindings bo_bindings,
                                                       GLuint                             texture_unit);

/** TODO */
PUBLIC void ogl_context_sampler_bindings_init(ogl_context_sampler_bindings              bindings,
                                              const ogl_context_gl_entrypoints_private* entrypoints_private_ptr);

/** TODO */
PUBLIC void ogl_context_sampler_bindings_release(ogl_context_sampler_bindings bindings);

/** TODO */
PUBLIC void ogl_context_sampler_bindings_set_binding(ogl_context_sampler_bindings bindings,
                                                     GLuint                       texture_unit,
                                                     GLuint                       sampler);

/** TODO
 *
 *  @param bindings  TODO
 *
 */
PUBLIC void ogl_context_sampler_bindings_sync(ogl_context_sampler_bindings bindings);

#endif /* OGL_CONTEXT_SAMPLER_BINDINGS_H */
