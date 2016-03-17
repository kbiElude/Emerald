#ifndef RAGL_TYPES_H
#define RAGL_TYPES_H

#include "system/system_types.h"


DECLARE_HANDLE(raGL_backend);
DECLARE_HANDLE(raGL_buffer);
DECLARE_HANDLE(raGL_framebuffer);
DECLARE_HANDLE(raGL_program);
DECLARE_HANDLE(raGL_sampler);
DECLARE_HANDLE(raGL_shader);
DECLARE_HANDLE(raGL_sync);
DECLARE_HANDLE(raGL_texture);

typedef enum
{
    /* not settable, uint32_t */
    RAGL_BUFFERS_BUFFER_PROPERTY_SIZE,
} raGL_buffers_buffer_property;

#endif /* RAGL_TYPES_H */