/**
 *
 * Emerald (kbi/elude @2014)
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

    GLuint active_program_context;
    GLuint active_program_local;

    GLint active_scissor_box_context[4];
    GLint active_scissor_box_local  [4];

    GLuint active_texture_unit_context;
    GLuint active_texture_unit_local;

    GLuint active_vertex_array_object_context;
    GLuint active_vertex_array_object_local;

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

    ASSERT_ALWAYS_SYNC(new_cache != NULL, "Out of memory");
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
        case OGL_CONTEXT_STATE_CACHE_PROPERTY_SCISSOR_BOX:
        {
            memcpy(out_result,
                   cache_ptr->active_scissor_box_local,
                   sizeof(cache_ptr->active_scissor_box_local) );

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_PROGRAM_OBJECT:
        {
            *((GLuint*) out_result) = cache_ptr->active_program_local;

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

    /* Set default state */
    memset(cache_ptr->active_clear_color_context, 0, sizeof(cache_ptr->active_clear_color_context) );
    memset(cache_ptr->active_clear_color_local,   0, sizeof(cache_ptr->active_clear_color_local) );

    cache_ptr->active_program_context      = 0;
    cache_ptr->active_program_local        = 0;

    cache_ptr->active_texture_unit_context = 0;
    cache_ptr->active_texture_unit_local   = 0;

    cache_ptr->active_vertex_array_object_context = 0;
    cache_ptr->active_vertex_array_object_local   = 0;

    entrypoints_private_ptr->pGLGetIntegerv(GL_SCISSOR_BOX,
                                            cache_ptr->active_scissor_box_context);
    memcpy                                 (cache_ptr->active_scissor_box_local,
                                            cache_ptr->active_scissor_box_context,
                                            sizeof(cache_ptr->active_scissor_box_local) );
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
                                                 __in __notnull void*                            data)
{
    _ogl_context_state_cache* cache_ptr = (_ogl_context_state_cache*) cache;

    switch (property)
    {
        case OGL_CONTEXT_STATE_CACHE_PROPERTY_SCISSOR_BOX:
        {
            memcpy(cache_ptr->active_scissor_box_local,
                   data,
                   sizeof(cache_ptr->active_scissor_box_local) );

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_CLEAR_COLOR:
        {
            memcpy(cache_ptr->active_clear_color_local,
                   data,
                   sizeof(cache_ptr->active_clear_color_local) );

            break;
        }

        case OGL_CONTEXT_STATE_CACHE_PROPERTY_PROGRAM_OBJECT:
        {
            cache_ptr->active_program_local = *(GLuint*) data;

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

        /* Program object */
        if ((sync_bits & STATE_CACHE_SYNC_BIT_ACTIVE_PROGRAM_OBJECT)             &&
            cache_ptr->active_program_context != cache_ptr->active_program_local)
        {
            cache_ptr->entrypoints_private_ptr->pGLUseProgram(cache_ptr->active_program_local);

            cache_ptr->active_program_context = cache_ptr->active_program_local;
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
    } /* if (cache != NULL) */
}