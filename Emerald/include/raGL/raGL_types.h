#ifndef RAGL_TYPES_H
#define RAGL_TYPES_H

#include "ogl/ogl_types.h"
#include "system/system_types.h"


DECLARE_HANDLE(raGL_backend);
DECLARE_HANDLE(raGL_buffer);
DECLARE_HANDLE(raGL_command_buffer);
DECLARE_HANDLE(raGL_dep_tracker);
DECLARE_HANDLE(raGL_framebuffer);
DECLARE_HANDLE(raGL_framebuffers);
DECLARE_HANDLE(raGL_program);
DECLARE_HANDLE(raGL_rendering_handler);
DECLARE_HANDLE(raGL_sampler);
DECLARE_HANDLE(raGL_shader);
DECLARE_HANDLE(raGL_sync);
DECLARE_HANDLE(raGL_texture);
DECLARE_HANDLE(raGL_vao);
DECLARE_HANDLE(raGL_vaos);

typedef enum
{
    /* not settable, uint32_t */
    RAGL_BUFFERS_BUFFER_PROPERTY_SIZE,
} raGL_buffers_buffer_property;

typedef void (*PFNRAGLCONTEXTCALLBACKFROMCONTEXTTHREADPROC)(ogl_context context,
                                                            void*       user_arg);


#endif /* RAGL_TYPES_H */