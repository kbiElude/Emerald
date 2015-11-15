#ifndef RAGL_SAMPLER_H
#define RAGL_SAMPLER_H


#include "ral/ral_types.h"
#include "system/system_types.h"


DECLARE_HANDLE(raGL_sampler);


typedef enum
{
    /* not settable; GLuint */
    RAGL_SAMPLER_PROPERTY_ID,

    /* not settable; ral_sampler */
    RAGL_SAMPLER_PROPERTY_SAMPLER,

} raGL_sampler_property;


/** TODO
 *
 *  NOTE: You should never need to use this function. Instead, route all your create() calls through
 *        raGL_samplers to re-use samplers which have already been created.
 *
 **/
PUBLIC raGL_sampler raGL_sampler_create(ogl_context context,
                                        GLint       sampler_id,
                                        ral_sampler sampler);

/** TODO */
PUBLIC void raGL_sampler_get_property(raGL_sampler          sampler,
                                      raGL_sampler_property property,
                                      void*                 out_result_ptr);

/** TODO */
PUBLIC void raGL_sampler_release(raGL_sampler sampler);


#endif /* RAGL_SAMPLER_H */