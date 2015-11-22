#ifndef RAGL_FRAMEBUFFER_H
#define RAGL_FRAMEBUFFER_H

#include "raGL/raGL_types.h"
#include "ral/ral_types.h"
#include "system/system_types.h"


/** TODO */
PUBLIC raGL_framebuffer raGL_framebuffer_create(ogl_context     context,
                                                GLint           fb_id,
                                                ral_framebuffer fb);

/** TODO */
PUBLIC void raGL_framebuffer_release(raGL_framebuffer fb);


#endif /* RAGL_FRAMEBUFFER_H */