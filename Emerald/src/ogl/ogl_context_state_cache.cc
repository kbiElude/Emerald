/**
 *
 * Emerald (kbi/elude @2014-2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_context_state_cache.h"
#include "ogl/ogl_sampler.h"
#include "system/system_hash64map.h"

/** TODO */
typedef struct _ogl_context_state_cache
{
    GLfloat active_clear_color_context[4];
    GLfloat active_clear_color_local  [4];

    GLdouble active_clear_depth_context;
    GLdouble active_clear_depth_local;

    GLboolean active_color_mask_context[4];
    GLboolean active_color_mask_local  [4];

    GLenum active_cull_face_context;
    GLenum active_cull_face_local;

    GLenum active_depth_func_context;
    GLenum active_depth_func_local;

    GLboolean active_depth_mask_context;
    GLboolean active_depth_mask_local;

    GLuint active_draw_fbo_context;
    GLuint active_draw_fbo_local;

    GLenum active_front_face_context;
    GLenum active_front_face_local;

    GLuint active_read_fbo_context;
    GLuint active_read_fbo_local;

    GLuint active_program_context;
    GLuint active_program_local;

    GLint active_scissor_box_context[4];
    GLint active_scissor_box_local  [4];

    GLuint active_texture_unit_context;
    GLuint active_texture_unit_local;

    GLuint active_vertex_array_object_context;
    GLuint active_vertex_array_object_local;

    GLint  active_viewport_context[4];
    GLint  active_viewport_local  [4];

    /* Blending */
    GLfloat blend_color_context[4];
    GLfloat blend_color_local  [4];
    GLenum  blend_equation_alpha_context;
    GLenum  blend_equation_alpha_local;
    GLenum  blend_equation_rgb_context;
    GLenum  blend_equation_rgb_local;
    GLenum  blend_function_dst_alpha_context;
    GLenum  blend_function_dst_alpha_local;
    GLenum  blend_function_dst_rgb_context;
    GLenum  blend_function_dst_rgb_local;
    GLenum  blend_function_src_alpha_context;
    GLenum  blend_function_src_alpha_local;
    GLenum  blend_function_src_rgb_context;
    GLenum  blend_function_src_rgb_local;
    bool    is_blend_mode_enabled_context;
    bool    is_blend_mode_enabled_local;

    /* DO NOT retain/release, as this object is managed by ogl_context and retaining it
     * will cause the rendering context to never release itself.
     */
    ogl_context context;

    const ogl_context_gl_entrypoints_private* entrypoints_private_ptr;
} _ogl_context_state_cache;


/** Please see header for spec */
PUBLIC ogl_context_state_cache ogl_context_state_cache_create(__in __notnull ogl_context context)
{
    _ogl_context_state_cache* new_cache = new (std::nothrow) _ogl_context_state_cache;

    ASSERT_ALWAYS_SYNC(new_cache != NULL,
                       "Out of memory");

    if (new_cache != NULL)
    {
        new_cache->context = context;
    } /* if (new_bindings != NULL) */

    return (ogl_context_state_cache) new_cache;
}

/** Please see header for spec */
PUBLIC void ogl_context_state_cache_get_property(__in  __notnull const ogl_context_state_cache    cache,
                                                 __in            ogl_context_state_cache_property property,
                                                 __out __notnull void*                            out_result)
{
    const _ogl_context_state_cache* cache_ptr = (const _ogl_context_state_cache*) cache;

    switch (property)
    {
        case OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_COLOR:
        {
            memcpy(out_result,
                   cache_ptr->blend_color_local,
                   sizeof(cache_ptr->blend_color_local) );

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_EQUATION_ALPHA:
        {
            *(GLenum*) out_result = cache_ptr->blend_equation_alpha_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_EQUATION_RGB:
        {
            *(GLenum*) out_result = cache_ptr->blend_equation_rgb_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_FUNC_DST_ALPHA:
        {
            *(GLenum*) out_result = cache_ptr->blend_function_dst_alpha_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_FUNC_DST_RGB:
        {
            *(GLenum*) out_result = cache_ptr->blend_function_dst_rgb_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_FUNC_SRC_ALPHA:
        {
            *(GLenum*) out_result = cache_ptr->blend_function_src_alpha_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_FUNC_SRC_RGB:
        {
            *(GLenum*) out_result = cache_ptr->blend_function_src_rgb_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_MODE_ENABLED:
        {
            *(bool*) out_result = cache_ptr->is_blend_mode_enabled_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_COLOR_MASK:
        {
            memcpy(out_result,
                   cache_ptr->active_color_mask_local,
                   sizeof(cache_ptr->active_color_mask_local) );

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_CULL_FACE:
        {
            *(GLenum*) out_result = cache_ptr->active_cull_face_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_DEPTH_FUNC:
        {
            *(GLenum*) out_result = cache_ptr->active_depth_func_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_DEPTH_MASK:
        {
            *(GLboolean*) out_result = cache_ptr->active_depth_mask_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_DRAW_FRAMEBUFFER:
        {
            *(GLuint*) out_result = cache_ptr->active_draw_fbo_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_FRONT_FACE:
        {
            *(GLuint*) out_result = cache_ptr->active_front_face_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_PROGRAM_OBJECT:
        {
            *((GLuint*) out_result) = cache_ptr->active_program_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_READ_FRAMEBUFFER:
        {
            *(GLuint*) out_result = cache_ptr->active_read_fbo_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_SCISSOR_BOX:
        {
            memcpy(out_result,
                   cache_ptr->active_scissor_box_local,
                   sizeof(cache_ptr->active_scissor_box_local) );

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_TEXTURE_UNIT:
        {
            *((GLuint*) out_result) = cache_ptr->active_texture_unit_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_VERTEX_ARRAY_OBJECT:
        {
            *((GLuint*) out_result) = cache_ptr->active_vertex_array_object_local;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_VIEWPORT:
        {
            memcpy(out_result,
                   cache_ptr->active_viewport_local,
                   sizeof(cache_ptr->active_viewport_local) );

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ogl_context_state_cache property");
        }
    } /* switch (property) */
}

/** Please see header for spec */
PUBLIC void ogl_context_state_cache_init(__in __notnull ogl_context_state_cache                   cache,
                                         __in __notnull const ogl_context_gl_entrypoints_private* entrypoints_private_ptr)
{
    _ogl_context_state_cache*    cache_ptr  = (_ogl_context_state_cache*) cache;
    const ogl_context_gl_limits* limits_ptr = NULL;

    /* Cache info in private descriptor */
    cache_ptr->entrypoints_private_ptr = entrypoints_private_ptr;

    /* Set default state: active color mask */
    cache_ptr->active_color_mask_context[0] = GL_TRUE;
    cache_ptr->active_color_mask_context[1] = GL_TRUE;
    cache_ptr->active_color_mask_context[2] = GL_TRUE;
    cache_ptr->active_color_mask_context[3] = GL_TRUE;

    memcpy(cache_ptr->active_color_mask_local,
           cache_ptr->active_color_mask_context,
           sizeof(cache_ptr->active_color_mask_local) );

    /* Set default state: active depth mask */
    cache_ptr->active_depth_mask_context = GL_TRUE;
    cache_ptr->active_depth_mask_local   = GL_TRUE;

    /* Set default state: active draw framebuffer */
    cache_ptr->active_draw_fbo_context = 0;
    cache_ptr->active_draw_fbo_local   = 0;

    /* Set default state: active front face */
    cache_ptr->active_front_face_context = GL_CCW;
    cache_ptr->active_front_face_local   = GL_CCW;

    /* Set default state: active program */
    cache_ptr->active_program_context      = 0;
    cache_ptr->active_program_local        = 0;

    /* Set default state: active read framebuffer */
    cache_ptr->active_read_fbo_context = 0;
    cache_ptr->active_read_fbo_local   = 0;

    /* Set default state: active texture unit */
    cache_ptr->active_texture_unit_context = 0;
    cache_ptr->active_texture_unit_local   = 0;

    /* Set default state: active VAO */
    cache_ptr->active_vertex_array_object_context = 0;
    cache_ptr->active_vertex_array_object_local   = 0;

    /* Set default state: blend color */
    memset(cache_ptr->blend_color_context,
           0,
           sizeof(cache_ptr->blend_color_context) );
    memset(cache_ptr->blend_color_local,
           0,
           sizeof(cache_ptr->blend_color_local) );

    /* Set default state: blend equation (alpha) */
    cache_ptr->blend_equation_alpha_context = GL_FUNC_ADD;
    cache_ptr->blend_equation_alpha_local   = GL_FUNC_ADD;

    /* Set default state: blend equation (RGB) */
    cache_ptr->blend_equation_rgb_context = GL_FUNC_ADD;
    cache_ptr->blend_equation_rgb_local   = GL_FUNC_ADD;

    /* Set default state: blend function (destination, alpha) */
    cache_ptr->blend_function_dst_alpha_context = GL_ZERO;
    cache_ptr->blend_function_dst_alpha_local   = GL_ZERO;

    /* Set default state: blend function (destination, RGB) */
    cache_ptr->blend_function_dst_rgb_context = GL_ZERO;
    cache_ptr->blend_function_dst_rgb_local   = GL_ZERO;

    /* Set default state: blend function (source, alpha) */
    cache_ptr->blend_function_src_alpha_context = GL_ONE;
    cache_ptr->blend_function_src_alpha_local   = GL_ONE;

    /* Set default state: blend function (source, RGB) */
    cache_ptr->blend_function_src_rgb_context = GL_ONE;
    cache_ptr->blend_function_src_rgb_local   = GL_ONE;

    /* Set default state: blend rendering mode */
    cache_ptr->is_blend_mode_enabled_context = false;
    cache_ptr->is_blend_mode_enabled_local   = false;

    /* Set default state: clear color */
    memset(cache_ptr->active_clear_color_context,
           0,
           sizeof(cache_ptr->active_clear_color_context) );
    memset(cache_ptr->active_clear_color_local,
           0,
           sizeof(cache_ptr->active_clear_color_local) );

    /* Set default state: clear depth value */
    cache_ptr->active_clear_depth_context = 1.0f;
    cache_ptr->active_clear_depth_local   = 1.0f;

    /* Set default state: cull face */
    cache_ptr->active_cull_face_context = GL_BACK;
    cache_ptr->active_cull_face_local   = GL_BACK;

    /* Set default state: depth func */
    cache_ptr->active_depth_func_context = GL_LESS;
    cache_ptr->active_depth_func_local   = GL_LESS;

    /* Set default state: scissor box */
    entrypoints_private_ptr->pGLGetIntegerv(GL_SCISSOR_BOX,
                                            cache_ptr->active_scissor_box_context);
    memcpy                                 (cache_ptr->active_scissor_box_local,
                                            cache_ptr->active_scissor_box_context,
                                            sizeof(cache_ptr->active_scissor_box_local) );

    /* Set default state: viewport */
    entrypoints_private_ptr->pGLGetIntegerv(GL_VIEWPORT,
                                            cache_ptr->active_viewport_context);
    memcpy                                 (cache_ptr->active_viewport_local,
                                            cache_ptr->active_viewport_context,
                                            sizeof(cache_ptr->active_viewport_local) );
}

/** Please see header for spec */
PUBLIC void ogl_context_state_cache_release(__in __notnull __post_invalid ogl_context_state_cache cache)
{
    _ogl_context_state_cache* cache_ptr = (_ogl_context_state_cache*) cache;

    /* Done */
    delete cache_ptr;

    cache_ptr = NULL;
}

/* Please see header for spec */
PUBLIC void ogl_context_state_cache_set_property(__in __notnull ogl_context_state_cache          cache,
                                                 __in           ogl_context_state_cache_property property,
                                                 __in __notnull const void*                      data)
{
    _ogl_context_state_cache* cache_ptr = (_ogl_context_state_cache*) cache;

    switch (property)
    {
        case OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_COLOR:
        {
            memcpy(cache_ptr->blend_color_local,
                   data,
                   sizeof(cache_ptr->blend_color_local) );

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_EQUATION_ALPHA:
        {
            cache_ptr->blend_equation_alpha_local = *(GLenum*) data;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_EQUATION_RGB:
        {
            cache_ptr->blend_equation_rgb_local = *(GLenum*) data;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_FUNC_DST_ALPHA:
        {
            cache_ptr->blend_function_dst_alpha_local = *(GLenum*) data;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_FUNC_DST_RGB:
        {
            cache_ptr->blend_function_dst_rgb_local = *(GLenum*) data;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_FUNC_SRC_ALPHA:
        {
            cache_ptr->blend_function_src_alpha_local = *(GLenum*) data;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_FUNC_SRC_RGB:
        {
            cache_ptr->blend_function_src_rgb_local = *(GLenum*) data;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_BLEND_MODE_ENABLED:
        {
            cache_ptr->is_blend_mode_enabled_local = *(bool*) data;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_CLEAR_COLOR:
        {
            memcpy(cache_ptr->active_clear_color_local,
                   data,
                   sizeof(cache_ptr->active_clear_color_local) );

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_CLEAR_DEPTH:
        {
            cache_ptr->active_clear_depth_local = *(GLdouble*) data;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_COLOR_MASK:
        {
            memcpy(cache_ptr->active_color_mask_local,
                   data,
                   sizeof(cache_ptr->active_color_mask_local) );

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_CULL_FACE:
        {
            cache_ptr->active_cull_face_local = *(GLenum*) data;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_DEPTH_FUNC:
        {
            cache_ptr->active_depth_func_local = *(GLenum*) data;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_DEPTH_MASK:
        {
            cache_ptr->active_depth_mask_local = *(GLboolean*) data;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_DRAW_FRAMEBUFFER:
        {
            cache_ptr->active_draw_fbo_local = *(GLuint*) data;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_FRONT_FACE:
        {
            cache_ptr->active_front_face_local = *(GLenum*) data;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_PROGRAM_OBJECT:
        {
            cache_ptr->active_program_local = *(GLuint*) data;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_READ_FRAMEBUFFER:
        {
            cache_ptr->active_read_fbo_local = *(GLuint*) data;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_SCISSOR_BOX:
        {
            memcpy(cache_ptr->active_scissor_box_local,
                   data,
                   sizeof(cache_ptr->active_scissor_box_local) );

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_TEXTURE_UNIT:
        {
            cache_ptr->active_texture_unit_local = *(GLuint*) data;

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_VERTEX_ARRAY_OBJECT:
        {
            cache_ptr->active_vertex_array_object_local = *((GLuint*) data);

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_VIEWPORT:
        {
            memcpy(cache_ptr->active_viewport_local,
                   data,
                   sizeof(cache_ptr->active_viewport_local) );

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ogl_context_state_cache property");
        }
    } /* switch (property) */
}

/** Please see header for spec */
PUBLIC void ogl_context_state_cache_sync(__in __notnull ogl_context_state_cache cache,
                                         __in           uint32_t                sync_bits)
{
    /* NOTE: cache is NULL during rendering context initialization */
    if (cache != NULL)
    {
        _ogl_context_state_cache* cache_ptr = (_ogl_context_state_cache*) cache;

        /* Blending configuration */
        if ((sync_bits & STATE_CACHE_SYNC_BIT_BLENDING) )
        {
            if (cache_ptr->blend_function_dst_alpha_context != cache_ptr->blend_function_dst_alpha_local ||
                cache_ptr->blend_function_dst_rgb_context   != cache_ptr->blend_function_dst_rgb_local   ||
                cache_ptr->blend_function_src_alpha_context != cache_ptr->blend_function_src_alpha_local ||
                cache_ptr->blend_function_src_rgb_context   != cache_ptr->blend_function_src_rgb_local)
            {
                cache_ptr->entrypoints_private_ptr->pGLBlendFuncSeparate(cache_ptr->blend_function_src_rgb_local,
                                                                         cache_ptr->blend_function_dst_rgb_local,
                                                                         cache_ptr->blend_function_src_alpha_local,
                                                                         cache_ptr->blend_function_dst_alpha_local);

                cache_ptr->blend_function_dst_alpha_context = cache_ptr->blend_function_dst_alpha_local;
                cache_ptr->blend_function_dst_rgb_context   = cache_ptr->blend_function_dst_rgb_local;
                cache_ptr->blend_function_src_alpha_context = cache_ptr->blend_function_src_alpha_local;
                cache_ptr->blend_function_src_rgb_context   = cache_ptr->blend_function_src_rgb_local;
            }

            if (cache_ptr->blend_equation_rgb_context   != cache_ptr->blend_equation_rgb_local ||
                cache_ptr->blend_equation_alpha_context != cache_ptr->blend_equation_alpha_local)
            {
                cache_ptr->entrypoints_private_ptr->pGLBlendEquationSeparate(cache_ptr->blend_equation_rgb_local,
                                                                             cache_ptr->blend_equation_alpha_local);

                cache_ptr->blend_equation_alpha_context = cache_ptr->blend_equation_alpha_local;
                cache_ptr->blend_equation_rgb_context   = cache_ptr->blend_equation_rgb_local;
            }

            if (memcmp(cache_ptr->blend_color_local,
                       cache_ptr->blend_color_context,
                       sizeof(cache_ptr->blend_color_local) ) != 0)
            {
                cache_ptr->entrypoints_private_ptr->pGLBlendColor(cache_ptr->blend_color_local[0],
                                                                  cache_ptr->blend_color_local[1],
                                                                  cache_ptr->blend_color_local[2],
                                                                  cache_ptr->blend_color_local[3]);
            }

            if (cache_ptr->is_blend_mode_enabled_context != cache_ptr->is_blend_mode_enabled_local)
            {
                if (cache_ptr->is_blend_mode_enabled_local)
                {
                    cache_ptr->entrypoints_private_ptr->pGLEnable(GL_BLEND);
                }
                else
                {
                    cache_ptr->entrypoints_private_ptr->pGLDisable(GL_BLEND);
                }

                cache_ptr->is_blend_mode_enabled_context = cache_ptr->is_blend_mode_enabled_local;
            }
        }

        /* Clear color */
        if ((sync_bits & STATE_CACHE_SYNC_BIT_ACTIVE_CLEAR_COLOR) &&
            (fabs(cache_ptr->active_clear_color_context[0] - cache_ptr->active_clear_color_local[0]) > 1e-5f ||
             fabs(cache_ptr->active_clear_color_context[1] - cache_ptr->active_clear_color_local[1]) > 1e-5f ||
             fabs(cache_ptr->active_clear_color_context[2] - cache_ptr->active_clear_color_local[2]) > 1e-5f ||
             fabs(cache_ptr->active_clear_color_context[3] - cache_ptr->active_clear_color_local[3]) > 1e-5f))
        {
            cache_ptr->entrypoints_private_ptr->pGLClearColor(cache_ptr->active_clear_color_local[0],
                                                              cache_ptr->active_clear_color_local[1],
                                                              cache_ptr->active_clear_color_local[2],
                                                              cache_ptr->active_clear_color_local[3]);

            memcpy(cache_ptr->active_clear_color_context,
                   cache_ptr->active_clear_color_local,
                   sizeof(cache_ptr->active_clear_color_context) );
        }

        /* Clear depth */
        if ((sync_bits & STATE_CACHE_SYNC_BIT_ACTIVE_CLEAR_DEPTH) &&
            fabs(cache_ptr->active_clear_depth_context - cache_ptr->active_clear_depth_local) > 1e-5f)
        {
            cache_ptr->entrypoints_private_ptr->pGLClearDepth(cache_ptr->active_clear_depth_local);

            cache_ptr->active_clear_depth_context = cache_ptr->active_clear_depth_local;
        }

        /* Color / depth mask */
        if (sync_bits & STATE_CACHE_SYNC_BIT_ACTIVE_COLOR_DEPTH_MASK)
        {
            if (memcmp(cache_ptr->active_color_mask_context,
                       cache_ptr->active_color_mask_local,
                       sizeof(cache_ptr->active_color_mask_context)) != 0)
            {
                cache_ptr->entrypoints_private_ptr->pGLColorMask(cache_ptr->active_color_mask_local[0],
                                                                 cache_ptr->active_color_mask_local[1],
                                                                 cache_ptr->active_color_mask_local[2],
                                                                 cache_ptr->active_color_mask_local[3]);

                memcpy(cache_ptr->active_color_mask_context,
                       cache_ptr->active_color_mask_local,
                       sizeof(cache_ptr->active_color_mask_context) );
            }

            if (cache_ptr->active_depth_mask_context != cache_ptr->active_depth_mask_local)
            {
                cache_ptr->entrypoints_private_ptr->pGLDepthMask(cache_ptr->active_depth_mask_local);

                cache_ptr->active_depth_mask_context = cache_ptr->active_depth_mask_local;
            }
        }

        /* Cull face */
        if (sync_bits & STATE_CACHE_SYNC_BIT_ACTIVE_CULL_FACE                        &&
            cache_ptr->active_cull_face_context != cache_ptr->active_cull_face_local)
        {
            cache_ptr->entrypoints_private_ptr->pGLCullFace(cache_ptr->active_cull_face_local);

            cache_ptr->active_cull_face_context = cache_ptr->active_cull_face_local;
        }

        /* Depth func */
        if (sync_bits & STATE_CACHE_SYNC_BIT_ACTIVE_DEPTH_FUNC &&
            cache_ptr->active_depth_func_context != cache_ptr->active_depth_func_local)
        {
            cache_ptr->entrypoints_private_ptr->pGLDepthFunc(cache_ptr->active_depth_func_local);

            cache_ptr->active_depth_func_context = cache_ptr->active_depth_func_local;
        }

        /* Draw framebuffer */
        if (sync_bits & STATE_CACHE_SYNC_BIT_ACTIVE_DRAW_FRAMEBUFFER &&
            cache_ptr->active_draw_fbo_context != cache_ptr->active_draw_fbo_local)
        {
            cache_ptr->entrypoints_private_ptr->pGLBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                                                                   cache_ptr->active_draw_fbo_local);

            cache_ptr->active_draw_fbo_context = cache_ptr->active_draw_fbo_local;
        }

        /* Front face */
        if (sync_bits & STATE_CACHE_SYNC_BIT_ACTIVE_FRONT_FACE &&
            cache_ptr->active_front_face_context != cache_ptr->active_front_face_local)
        {
            cache_ptr->entrypoints_private_ptr->pGLFrontFace(cache_ptr->active_front_face_local);

            cache_ptr->active_front_face_context = cache_ptr->active_front_face_local;
        }

        /* Program object */
        if ((sync_bits & STATE_CACHE_SYNC_BIT_ACTIVE_PROGRAM_OBJECT)             &&
            cache_ptr->active_program_context != cache_ptr->active_program_local)
        {
            cache_ptr->entrypoints_private_ptr->pGLUseProgram(cache_ptr->active_program_local);

            cache_ptr->active_program_context = cache_ptr->active_program_local;
        }

        /* Read framebuffer */
        if (sync_bits & STATE_CACHE_SYNC_BIT_ACTIVE_READ_FRAMEBUFFER &&
            cache_ptr->active_read_fbo_context != cache_ptr->active_read_fbo_local)
        {
            cache_ptr->entrypoints_private_ptr->pGLBindFramebuffer(GL_READ_FRAMEBUFFER,
                                                                   cache_ptr->active_read_fbo_local);

            cache_ptr->active_read_fbo_context = cache_ptr->active_read_fbo_local;
        }

        /* Scissor box */
        if ((sync_bits & STATE_CACHE_SYNC_BIT_ACTIVE_SCISSOR_BOX) &&
            (cache_ptr->active_scissor_box_context[0] != cache_ptr->active_scissor_box_local[0] ||
             cache_ptr->active_scissor_box_context[1] != cache_ptr->active_scissor_box_local[1] ||
             cache_ptr->active_scissor_box_context[2] != cache_ptr->active_scissor_box_local[2] ||
             cache_ptr->active_scissor_box_context[3] != cache_ptr->active_scissor_box_local[3]))
        {
            cache_ptr->entrypoints_private_ptr->pGLScissor(cache_ptr->active_scissor_box_local[0],
                                                           cache_ptr->active_scissor_box_local[1],
                                                           cache_ptr->active_scissor_box_local[2],
                                                           cache_ptr->active_scissor_box_local[3]);

            memcpy(cache_ptr->active_scissor_box_context,
                   cache_ptr->active_scissor_box_local,
                   sizeof(cache_ptr->active_scissor_box_context) );
        }

        /* Texture unit */
        if ((sync_bits & STATE_CACHE_SYNC_BIT_ACTIVE_TEXTURE_UNIT)                         &&
            cache_ptr->active_texture_unit_context != cache_ptr->active_texture_unit_local)
        {
            cache_ptr->entrypoints_private_ptr->pGLActiveTexture(GL_TEXTURE0 + cache_ptr->active_texture_unit_local);

            cache_ptr->active_texture_unit_context = cache_ptr->active_texture_unit_local;
        }

        /* Vertex array object */
        if ((sync_bits & STATE_CACHE_SYNC_BIT_ACTIVE_VERTEX_ARRAY_OBJECT)                                 &&
            cache_ptr->active_vertex_array_object_context != cache_ptr->active_vertex_array_object_local)
        {
            cache_ptr->entrypoints_private_ptr->pGLBindVertexArray(cache_ptr->active_vertex_array_object_local);

            cache_ptr->active_vertex_array_object_context = cache_ptr->active_vertex_array_object_local;
        }

        /* Viewport */
        if ((sync_bits & STATE_CACHE_SYNC_BIT_ACTIVE_VIEWPORT)                            &&
            (cache_ptr->active_viewport_context[0] != cache_ptr->active_viewport_local[0] ||
             cache_ptr->active_viewport_context[1] != cache_ptr->active_viewport_local[1] ||
             cache_ptr->active_viewport_context[2] != cache_ptr->active_viewport_local[2] ||
             cache_ptr->active_viewport_context[3] != cache_ptr->active_viewport_local[3] ))
        {
            cache_ptr->entrypoints_private_ptr->pGLViewport(cache_ptr->active_viewport_local[0],
                                                            cache_ptr->active_viewport_local[1],
                                                            cache_ptr->active_viewport_local[2],
                                                            cache_ptr->active_viewport_local[3]);

            memcpy(cache_ptr->active_viewport_context,
                   cache_ptr->active_viewport_local,
                   sizeof(cache_ptr->active_viewport_local) );
        }
    } /* if (cache != NULL) */
}