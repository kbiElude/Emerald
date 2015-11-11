#ifndef RAGL_FRAMEBUFFER_H
#define RAGL_FRAMEBUFFER_H

#include "ral/ral_types.h"
#include "system/system_types.h"


DECLARE_HANDLE(raGL_framebuffer);

/** TODO */
PUBLIC raGL_framebuffer raGL_framebuffer_create(ogl_context     context,
                                                GLint           fb_id,
                                                ral_framebuffer fb);

/** TODO */
PUBLIC void raGL_framebuffer_release(raGL_framebuffer fb);


#endif /* RAGL_FRAMEBUFFER_H */