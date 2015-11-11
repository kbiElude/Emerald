#ifndef RAGL_BACKEND_H
#define RAGL_BACKEND_H

#include "ral/ral_types.h"
#include "system/system_types.h"


DECLARE_HANDLE(raGL_backend);


/** TODO */
PUBLIC raGL_backend raGL_backend_create(ral_context context);

/** TODO */
PUBLIC bool raGL_backend_get_framebuffer(void*           backend,
                                         ral_framebuffer framebuffer_ral,
                                         void**          out_framebuffer_backend_ptr);

/** TODO */
PUBLIC void raGL_backend_get_property(void*                backend, /* raGL_backend instance */
                                      ral_context_property property,
                                      void*                out_result_ptr);

/** TODO */
PUBLIC void raGL_backend_release(void* backend); /* raGL_backend instance */


#endif /* RAGL_BACKEND_H */