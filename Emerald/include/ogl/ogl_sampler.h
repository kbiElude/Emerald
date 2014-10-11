/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#ifndef OGL_SAMPLER_H
#define OGL_SAMPLER_H

#include "ogl/ogl_types.h"

REFCOUNT_INSERT_DECLARATIONS(ogl_sampler, ogl_sampler)


typedef enum _ogl_sampler_property
{
    OGL_SAMPLER_PROPERTY_BORDER_COLOR,         /* float[4] */
    OGL_SAMPLER_PROPERTY_MAG_FILTER,           /* GLenum */
    OGL_SAMPLER_PROPERTY_MAX_LOD,              /* float */
    OGL_SAMPLER_PROPERTY_MIN_FILTER,           /* GLenum */
    OGL_SAMPLER_PROPERTY_MIN_LOD,              /* float */
    OGL_SAMPLER_PROPERTY_TEXTURE_COMPARE_FUNC, /* GLenum */
    OGL_SAMPLER_PROPERTY_TEXTURE_COMPARE_MODE, /* GLenum */
    OGL_SAMPLER_PROPERTY_WRAP_R,               /* GLenum */
    OGL_SAMPLER_PROPERTY_WRAP_S,               /* GLenum */
    OGL_SAMPLER_PROPERTY_WRAP_T,               /* GLenum */

    OGL_SAMPLER_PROPERTY_LOCKED,               /* bool */

    /* Always last */
    OGL_SAMPLER_PROPERTY_COUNT
} ogl_sampler_property;

/** TODO */
PUBLIC EMERALD_API ogl_sampler ogl_sampler_create(__in __notnull ogl_context               context,
                                                  __in __notnull system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API GLuint ogl_sampler_get_id(__in __notnull ogl_sampler sampler);

/** TODO */
PUBLIC EMERALD_API void ogl_sampler_get_property(__in  __notnull const ogl_sampler    sampler,
                                                 __in            ogl_sampler_property property,
                                                 __out           void*                out_result);

/** TODO */
PUBLIC EMERALD_API void ogl_sampler_set_property(__in __notnull ogl_sampler          sampler,
                                                 __in           ogl_sampler_property property,
                                                 __in           const void*          in_data);
#endif /* OGL_SAMPLER_H */
