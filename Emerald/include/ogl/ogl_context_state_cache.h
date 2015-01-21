/**
 *
 * Emerald (kbi/elude @2014-2015)
 *
 * Helps cache general state information. Note that these functions are private only.
 */
#ifndef OGL_CONTEXT_STATE_CACHE_H
#define OGL_CONTEXT_STATE_CACHE_H

#include "ogl/ogl_types.h"

enum ogl_context_state_cache_property
{
    OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_COLOR,          /* settable, GLfloat[4] */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_EQUATION_ALPHA, /* settable, GLenum */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_EQUATION_RGB,   /* settable, GLenum */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_FUNC_DST_ALPHA, /* settable, GLenum */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_FUNC_DST_RGB,   /* settable, GLenum */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_FUNC_SRC_ALPHA, /* settable, GLenum */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_FUNC_SRC_RGB,   /* settable, GLenum */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_MODE_ENABLED,   /* settable, bool */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_CLEAR_COLOR,          /* settable, GLfloat[4] */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_PROGRAM_OBJECT,       /* settable, GLuint */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_SCISSOR_BOX,          /* settable, GLint[4] */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_TEXTURE_UNIT,         /* settable, GLuint (GL_TEXTURE0 = 0, ..) */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_VERTEX_ARRAY_OBJECT   /* settable, GLuint */
};

enum ogl_context_state_cache_sync_bit
{
    STATE_CACHE_SYNC_BIT_ACTIVE_TEXTURE_UNIT        = 1 << 0,
    STATE_CACHE_SYNC_BIT_ACTIVE_PROGRAM_OBJECT      = 1 << 1,
    STATE_CACHE_SYNC_BIT_ACTIVE_VERTEX_ARRAY_OBJECT = 1 << 2,
    STATE_CACHE_SYNC_BIT_ACTIVE_CLEAR_COLOR         = 1 << 3,
    STATE_CACHE_SYNC_BIT_ACTIVE_SCISSOR_BOX         = 1 << 4,
    STATE_CACHE_SYNC_BIT_BLENDING                   = 1 << 5,
    STATE_CACHE_SYNC_BIT_ALL                        = 0xFFFFFFFF
};

/* Declare private handle */
DECLARE_HANDLE(ogl_context_state_cache);

/** TODO */
PUBLIC ogl_context_state_cache ogl_context_state_cache_create(__in __notnull ogl_context context);

/** TODO */
PUBLIC void ogl_context_state_cache_get_property(__in  __notnull const ogl_context_state_cache    cache,
                                                 __in            ogl_context_state_cache_property property,
                                                 __out __notnull void*                            out_result);

/** TODO */
PUBLIC void ogl_context_state_cache_init(__in __notnull ogl_context_state_cache                   cache,
                                         __in __notnull const ogl_context_gl_entrypoints_private* entrypoints_private_ptr);

/** TODO */
PUBLIC void ogl_context_state_cache_release(__in __notnull __post_invalid ogl_context_state_cache cache);

/** TODO */
PUBLIC void ogl_context_state_cache_set_property(__in __notnull ogl_context_state_cache          cache,
                                                 __in           ogl_context_state_cache_property property,
                                                 __in __notnull const void*                      data);

/** TODO
 *
 *  @param cache  TODO
 *
 */
PUBLIC void ogl_context_state_cache_sync(__in __notnull ogl_context_state_cache cache,
                                         __in           uint32_t                sync_bits);

#endif /* OGL_CONTEXT_STATE_CACHE_H */
