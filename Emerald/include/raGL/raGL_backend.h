#ifndef RAGL_BACKEND_H
#define RAGL_BACKEND_H

#include "ral/ral_types.h"
#include "system/system_types.h"


typedef enum
{
    /* not settable; raGL_buffers.
     *
     * Only ogl_context should use this property.
     **/
    RAGL_BACKEND_PRIVATE_PROPERTY_BUFFERS,

    /* not settable; raGL_textures.
     *
     * Only ogl_context should use this property.
     **/
    RAGL_BACKEND_PRIVATE_PROPERTY_TEXTURES,

} raGL_backend_private_property;


/** TODO */
PUBLIC raGL_backend raGL_backend_create(ral_context context);

/** TODO */
PUBLIC bool raGL_backend_get_buffer(void*      backend,
                                    ral_buffer buffer_ral,
                                    void**     out_buffer_raGL_ptr);

/** TODO */
PUBLIC bool raGL_backend_get_framebuffer(void*           backend,
                                         ral_framebuffer framebuffer_ral,
                                         void**          out_framebuffer_raGL_ptr);

/** TODO */
PUBLIC void raGL_backend_get_private_property(raGL_backend                  backend,
                                              raGL_backend_private_property property,
                                              void**                        out_result_ptr);

/** TODO */
PUBLIC void raGL_backend_get_property(void*                backend, /* raGL_backend instance */
                                      ral_context_property property,
                                      void*                out_result_ptr);

/** TODO */
PUBLIC bool raGL_backend_get_texture(void*       backend,
                                     ral_texture texture_ral,
                                     void**      out_texture_raGL_ptr);

/** TODO */
PUBLIC void raGL_backend_release(void* backend); /* raGL_backend instance */


#endif /* RAGL_BACKEND_H */