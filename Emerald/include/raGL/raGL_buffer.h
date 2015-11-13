#ifndef RAGL_BUFFER_H
#define RAGL_BUFFER_H

#include "ogl/ogl_types.h"
#include "ral/ral_types.h"
#include "system/system_types.h"


DECLARE_HANDLE(raGL_buffer);

/** TODO */
PUBLIC raGL_buffer raGL_buffer_create(ogl_context context,
                                      GLint       bo_id,
                                      ral_buffer  buffer);

/** TODO */
PUBLIC void raGL_buffer_release(raGL_buffer buffer);

#endif /* RAGL_BUFFER_H */