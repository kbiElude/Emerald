#ifndef RAGL_BACKEND_H
#define RAGL_BACKEND_H

#include "ogl/ogl_types.h"
#include "raGL/raGL_types.h"
#include "ral/ral_types.h"
#include "system/system_types.h"


/** TODO */
PUBLIC raGL_backend raGL_backend_create(ral_context               context,
                                        system_hashed_ansi_string name,
                                        ral_backend_type          type);

/** TODO */
PUBLIC bool raGL_backend_get_buffer(void*      backend,
                                    ral_buffer buffer_ral,
                                    void**     out_buffer_raGL_ptr);

/** TODO */
PUBLIC bool raGL_backend_get_framebuffer(void*           backend,
                                         ral_framebuffer framebuffer_ral,
                                         void**          out_framebuffer_raGL_ptr);

/** TODO */
PUBLIC ogl_context raGL_backend_get_root_context(ral_backend_type backend_type);

/** TODO */
PUBLIC void raGL_backend_get_property(void*                backend, /* raGL_backend instance */
                                      ral_context_property property,
                                      void*                out_result_ptr);

/** TODO */
PUBLIC bool raGL_backend_get_texture(void*       backend,
                                     ral_texture texture_ral,
                                     void**      out_texture_raGL_ptr);

PUBLIC bool raGL_backend_get_texture_by_id(raGL_backend  backend,
                                           GLuint        texture_id,
                                           raGL_texture* out_texture_raGL_ptr);

/** TODO */
PUBLIC void raGL_backend_release(void* backend); /* raGL_backend instance */


#endif /* RAGL_BACKEND_H */