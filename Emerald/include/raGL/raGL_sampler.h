#ifndef RAGL_SAMPLER_H
#define RAGL_SAMPLER_H


#include "ral/ral_types.h"
#include "system/system_types.h"


DECLARE_HANDLE(raGL_sampler);

/** TODO */
PUBLIC raGL_sampler raGL_sampler_create(ogl_context context,
                                        GLint       sampler_id,
                                        ral_sampler sampler);

/** TODO */
PUBLIC void raGL_sampler_release(raGL_sampler sampler);


#endif /* RAGL_SAMPLER_H */