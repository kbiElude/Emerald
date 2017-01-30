/**
 *
 * Emerald (kbi/elude @2014-2016)
 *
 * Helps cache general state information. Note that these functions are private only.
 */
#ifndef OGL_CONTEXT_STATE_CACHE_H
#define OGL_CONTEXT_STATE_CACHE_H

#include "ogl/ogl_types.h"

enum ogl_context_state_cache_indexed_property
{
    /* settable, GLint[4] */
    OGL_CONTEXT_STATE_CACHE_INDEXED_PROPERTY_SCISSOR_BOX,

    /* settable, GLfloat[4] */
    OGL_CONTEXT_STATE_CACHE_INDEXED_PROPERTY_VIEWPORT,
};

enum ogl_context_state_cache_property
{
    /* settable, GLfloat[4] */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_COLOR,

    /* settable, GLenum */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_EQUATION_ALPHA,

    /* settable, GLenum */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_EQUATION_RGB,

    /* settable, GLenum */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_FUNC_DST_ALPHA,

    /* settable, GLenum */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_FUNC_DST_RGB,

    /* settable, GLenum */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_FUNC_SRC_ALPHA,

    /* settable, GLenum */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_FUNC_SRC_RGB,

    /* settable, GLfloat[4] */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_CLEAR_COLOR,

    /* settable, GLdouble */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_CLEAR_DEPTH_DOUBLE,

    /* settable, GLfloat */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_CLEAR_DEPTH_FLOAT,

    /* settable, GLboolean[4] */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_COLOR_MASK,

    /* settable, GLenum */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_CULL_FACE,

    /* settable, GLenum */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_DEPTH_FUNC,

    /* settable, GLboolean */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_DEPTH_MASK,

    /* settable, GLuint */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_DRAW_FRAMEBUFFER,

    /* settable, GLenum */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_FRONT_FACE,

    /* settable, GLfloat */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_MIN_SAMPLE_SHADING,

    /* settable, GLint */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_N_PATCH_VERTICES,

    /* settable, GLfloat */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_POLYGON_OFFSET_FACTOR,

    /* settable, GLfloat */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_POLYGON_OFFSET_UNITS,

    /* settable, GLuint */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_PROGRAM_OBJECT,

    /* settable, GLuint */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_READ_FRAMEBUFFER,

    /* settable, GLuint */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERBUFFER,

    /* settable, GLboolean */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_BLEND,

    /* settable, GLboolean */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_COLOR_LOGIC_OP,

    /* settable, GLboolean */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_CULL_FACE,

    /* settable, GLboolean */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_DEPTH_CLAMP,

    /* settable, GLboolean */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_DEPTH_TEST,

    /* settable, GLboolean */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_DITHER,

    /* settable, GLboolean */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_FRAMEBUFFER_SRGB,

    /* settable, GLboolean */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_LINE_SMOOTH,

    /* settable, GLboolean */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_MULTISAMPLE,

    /* settable, GLboolean */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_POLYGON_OFFSET_FILL,

    /* settable, GLboolean */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_POLYGON_OFFSET_LINE,

    /* settable, GLboolean */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_POLYGON_OFFSET_POINT,

    /* settable, GLboolean */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_POLYGON_SMOOTH,

    /* settable, GLboolean */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_PRIMITIVE_RESTART,

    /* settable, GLboolean */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_PRIMITIVE_RESTART_FIXED_INDEX,

    /* settable, GLboolean */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_PROGRAM_POINT_SIZE,

    /* settable, GLboolean */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_RASTERIZER_DISCARD,

    /* settable, GLboolean */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_SAMPLE_ALPHA_TO_COVERAGE,

    /* settable, GLboolean */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_SAMPLE_ALPHA_TO_ONE,

    /* settable, GLboolean */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_SAMPLE_COVERAGE,

    /* settable, GLboolean */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_SAMPLE_SHADING,

    /* settable, GLboolean */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_SAMPLE_MASK,

    /* settable, GLboolean */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_SCISSOR_TEST,

    /* settable, GLboolean */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_STENCIL_TEST,

    /* settable, GLboolean */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_RENDERING_MODE_TEXTURE_CUBE_MAP_SEAMLESS,

    /* settable, GLuint (GL_TEXTURE0 = 0, ..) */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_TEXTURE_UNIT,

    /* settable, GLuint */
    OGL_CONTEXT_STATE_CACHE_PROPERTY_VERTEX_ARRAY_OBJECT,

    OGL_CONTEXT_STATE_CACHE_PROPERTY_UNKNOWN
};

enum ogl_context_state_cache_sync_bit
{
    STATE_CACHE_SYNC_BIT_ACTIVE_CLEAR_COLOR          = 1 << 0,
    STATE_CACHE_SYNC_BIT_ACTIVE_CLEAR_DEPTH          = 1 << 1,
    STATE_CACHE_SYNC_BIT_ACTIVE_COLOR_DEPTH_MASK     = 1 << 2,
    STATE_CACHE_SYNC_BIT_ACTIVE_CULL_FACE            = 1 << 3,
    STATE_CACHE_SYNC_BIT_ACTIVE_DEPTH_FUNC           = 1 << 4,
    STATE_CACHE_SYNC_BIT_ACTIVE_DRAW_FRAMEBUFFER     = 1 << 5,
    STATE_CACHE_SYNC_BIT_ACTIVE_FRONT_FACE           = 1 << 6,
    STATE_CACHE_SYNC_BIT_ACTIVE_MIN_SAMPLE_SHADING   = 1 << 7,
    STATE_CACHE_SYNC_BIT_ACTIVE_N_PATCH_VERTICES     = 1 << 8,
    STATE_CACHE_SYNC_BIT_ACTIVE_POLYGON_OFFSET_STATE = 1 << 9,
    STATE_CACHE_SYNC_BIT_ACTIVE_PROGRAM_OBJECT       = 1 << 10,
    STATE_CACHE_SYNC_BIT_ACTIVE_READ_FRAMEBUFFER     = 1 << 11,
    STATE_CACHE_SYNC_BIT_ACTIVE_RENDERBUFFER         = 1 << 12,
    STATE_CACHE_SYNC_BIT_ACTIVE_RENDERING_MODES      = 1 << 13,
    STATE_CACHE_SYNC_BIT_ACTIVE_SCISSOR_BOX          = 1 << 14,
    STATE_CACHE_SYNC_BIT_ACTIVE_TEXTURE_UNIT         = 1 << 15,
    STATE_CACHE_SYNC_BIT_ACTIVE_VERTEX_ARRAY_OBJECT  = 1 << 16,
    STATE_CACHE_SYNC_BIT_ACTIVE_VIEWPORT             = 1 << 17,
    STATE_CACHE_SYNC_BIT_BLENDING                    = 1 << 18,
    STATE_CACHE_SYNC_BIT_ALL                         = 0xFFFFFFFF
};

/* Declare private handle */
DECLARE_HANDLE(ogl_context_state_cache);

/** TODO */
PUBLIC ogl_context_state_cache ogl_context_state_cache_create(ogl_context context);

/** TODO */
PUBLIC void ogl_context_state_cache_get_indexed_property(const ogl_context_state_cache            cache,
                                                         ogl_context_state_cache_indexed_property property,
                                                         uint32_t                                 index,
                                                         void*                                    data);

/** TODO */
PUBLIC void ogl_context_state_cache_get_property(const ogl_context_state_cache    cache,
                                                 ogl_context_state_cache_property property,
                                                 void*                            out_result_ptr);

/** TODO */
PUBLIC void ogl_context_state_cache_init(ogl_context_state_cache                   cache,
                                         const ogl_context_gl_entrypoints_private* entrypoints_private_ptr,
                                         const ogl_context_gl_limits*              limits_ptr);

/** TODO */
PUBLIC void ogl_context_state_cache_release(ogl_context_state_cache cache);

/** TODO */
PUBLIC void ogl_context_state_cache_set_indexed_property(ogl_context_state_cache                  cache,
                                                         ogl_context_state_cache_indexed_property property,
                                                         uint32_t                                 index,
                                                         const void*                              data);

/** TODO */
PUBLIC void ogl_context_state_cache_set_property(ogl_context_state_cache          cache,
                                                 ogl_context_state_cache_property property,
                                                 const void*                      data);

/** TODO
 *
 *  @param cache  TODO
 *
 */
PUBLIC void ogl_context_state_cache_sync(ogl_context_state_cache cache,
                                         uint32_t                sync_bits);

#endif /* OGL_CONTEXT_STATE_CACHE_H */
